/* RFC1524 (mailcap file) implementation */

/* This file contains various functions for implementing a fair subset of
 * rfc1524.
 *
 * The rfc1524 defines a format for the Multimedia Mail Configuration, which is
 * the standard mailcap file format under Unix which specifies what external
 * programs should be used to view/compose/edit multimedia files based on
 * content type.
 *
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (c) 2002-2004 The ELinks project
 *
 * This file was hijacked from the Mutt project <URL:http://www.mutt.org>
 * (version 1.4) on Saturday the 7th December 2002. It has been heavily
 * elinksified. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "intl/libintl.h"
#include "main/module.h"
#include "mime/backend/common.h"
#include "mime/backend/mailcap.h"
#include "mime/mime.h"
#include "osdep/osdep.h"		/* For exe() */
#include "session/session.h"
#include "util/file.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"

struct mailcap_hash_item {
	/* The entries associated with the type */
	LIST_OF(struct mailcap_entry) entries;

	/* The content type of all @entries. Must be last! */
	char type[1];
};

struct mailcap_entry {
	LIST_HEAD_EL(struct mailcap_entry);

	/* To verify if command qualifies. Cannot contain %s formats. */
	char *testcommand;

	/* Used to inform the user of the type or handler. */
	char *description;

	/* Used to determine between an exact match and a wildtype match. Lower
	 * is better. Increased for each sourced file. */
	unsigned int priority;

	/* Whether the program "blocks" the term. */
	unsigned int needsterminal:1;

	/* If "| ${PAGER}" should be added. It would of course be better to
	 * pipe the output into a buffer and let ELinks display it but this
	 * will have to do for now. */
	unsigned int copiousoutput:1;
	unsigned int x_htmloutput:1;

	/* The 'raw' unformatted (view)command from the mailcap files. */
	char command[1];
};

enum mailcap_option {
	MAILCAP_TREE,

	MAILCAP_ENABLE,
	MAILCAP_PATH,
	MAILCAP_ASK,
	MAILCAP_DESCRIPTION,
	MAILCAP_PRIORITIZE,

	MAILCAP_OPTIONS
};

static union option_info mailcap_options[] = {
	INIT_OPT_TREE("mime", N_("Mailcap"),
		"mailcap", OPT_ZERO,
		N_("Options for mailcap support.")),

	INIT_OPT_BOOL("mime.mailcap", N_("Enable"),
		"enable", OPT_ZERO, 1,
		N_("Enable mailcap support.")),

	INIT_OPT_STRING("mime.mailcap", N_("Path"),
		"path", OPT_ZERO, DEFAULT_MAILCAP_PATH,
		N_("Mailcap search path. Colon-separated list of files. "
		"Leave as \"\" to use MAILCAP environment variable instead.")),

	INIT_OPT_BOOL("mime.mailcap", N_("Ask before opening"),
		"ask", OPT_ZERO, 1,
		N_("Ask before using the handlers defined by mailcap.")),

	INIT_OPT_INT("mime.mailcap", N_("Type query string"),
		"description", OPT_ZERO, 0, 2, 0,
		N_("Type of description to show in \"what to do with "
		"this file\" query dialog:\n"
		"0 is show \"mailcap\"\n"
		"1 is show program to be run\n"
		"2 is show mailcap description field if any;\n"
		"     \"mailcap\" otherwise")),

	INIT_OPT_BOOL("mime.mailcap", N_("Prioritize entries by file"),
		"prioritize", OPT_ZERO, 1,
		N_("Prioritize entries by the order of the files in "
		"the mailcap path. This means that wildcard entries "
		"(like: image/*) will also be checked before deciding "
		"the handler.")),

	NULL_OPTION_INFO,
};

#ifdef TEST_MAILCAP
#define get_opt_mailcap(which)		mailcap_options[(which)].init
#define get_mailcap(which)		get_opt_mailcap(which)
#define get_mailcap_ask()		get_mailcap(MAILCAP_ASK).value_long
#define get_mailcap_description()	get_mailcap(MAILCAP_DESCRIPTION).value_long
#define get_mailcap_enable()		get_mailcap(MAILCAP_ENABLE).value_long
#define get_mailcap_prioritize()	get_mailcap(MAILCAP_PRIORITIZE).value_long
#define get_mailcap_path()		get_mailcap(MAILCAP_PATH).value_dataptr
#else
#define get_opt_mailcap(which)		mailcap_options[(which)].option
#define get_mailcap(which)		get_opt_mailcap(which).value
#define get_mailcap_ask()		get_mailcap(MAILCAP_ASK).number
#define get_mailcap_description()	get_mailcap(MAILCAP_DESCRIPTION).number
#define get_mailcap_enable()		get_mailcap(MAILCAP_ENABLE).number
#define get_mailcap_prioritize()	get_mailcap(MAILCAP_PRIORITIZE).number
#define get_mailcap_path()		get_mailcap(MAILCAP_PATH).string
#endif

/* State variables */
static struct hash *mailcap_map = NULL;


static inline void
done_mailcap_entry(struct mailcap_entry *entry)
{
	ELOG
	if (!entry) return;
	mem_free_if(entry->testcommand);
	mem_free_if(entry->description);
	mem_free(entry);
}

/* Takes care of all initialization of mailcap entries.
 * Clear memory to make freeing it safer later and we get
 * needsterminal and copiousoutput initialized for free. */
static inline struct mailcap_entry *
init_mailcap_entry(char *command, int priority)
{
	ELOG
	struct mailcap_entry *entry;
	int commandlen = strlen(command);

	entry = (struct mailcap_entry *)mem_calloc(1, sizeof(*entry) + commandlen);
	if (!entry) return NULL;

	memcpy(entry->command, command, commandlen);

	entry->priority = priority;

	return entry;
}

static inline void
add_mailcap_entry(struct mailcap_entry *entry, char *type, int typelen)
{
	ELOG
	struct mailcap_hash_item *mitem;
	struct hash_item *item;

	/* Time to get the entry into the mailcap_map */
	/* First check if the type is already checked in */
	item = get_hash_item(mailcap_map, type, typelen);
	if (!item) {
		mitem = (struct mailcap_hash_item *)mem_alloc(sizeof(*mitem) + typelen);
		if (!mitem) {
			done_mailcap_entry(entry);
			return;
		}

		safe_strncpy(mitem->type, type, typelen + 1);

		init_list(mitem->entries);

		item = add_hash_item(mailcap_map, mitem->type, typelen, mitem);
		if (!item) {
			mem_free(mitem);
			done_mailcap_entry(entry);
			return;
		}
	} else if (item->value) {
		mitem = (struct mailcap_hash_item *)item->value;
	} else {
		done_mailcap_entry(entry);
		return;
	}

	add_to_list_end(mitem->entries, entry);
}

/* Parsing of a RFC1524 mailcap file */
/* The format is:
 *
 *	base/type; command; extradefs
 *
 * type can be * for matching all; base with no /type is an implicit
 * wildcard; command contains a %s for the filename to pass, default to pipe on
 * stdin; extradefs are of the form:
 *
 *	def1="definition"; def2="define \;";
 *
 * line wraps with a \ at the end of the line, # for comments. */
/* TODO handle default pipe. Maybe by prepending "cat |" to the command. */

/* Returns a NULL terminated RFC 1524 field, while modifying @next to point
 * to the next field. */
static char *
get_mailcap_field(char **next)
{
	ELOG
	char *field;
	char *fieldend;

	if (!next || !*next) return NULL;

	field = *next;
	skip_space(field);
	fieldend = field;

	/* End field at the next occurence of ';' but not escaped '\;' */
	do {
		/* Handle both if ';' is the first char or if it's escaped */
		if (*fieldend == ';')
			fieldend++;

		fieldend = strchr(fieldend, ';');
	} while (fieldend && *(fieldend-1) == '\\');

	if (fieldend) {
		*fieldend = '\0';
		*next = fieldend;
		fieldend--;
		(*next)++;
		skip_space(*next);
	} else {
		*next = NULL;
		fieldend = field + strlen(field) - 1;
	}

	/* Remove trailing whitespace */
	while (field <= fieldend && isspace((unsigned char)*fieldend))
		*fieldend-- = '\0';

	return field;
}

/* Parses specific fields (ex: the '=TestCommand' part of 'test=TestCommand').
 * Expects that @field is pointing right after the specifier (ex: 'test'
 * above). Allocates and returns a NULL terminated token, or NULL if parsing
 * fails. */

static char *
get_mailcap_field_text(char *field)
{
	ELOG
	skip_space(field);

	if (*field == '=') {
		field++;
		skip_space(field);

		return stracpy(field);
	}

	return NULL;
}

/* Parse optional extra definitions. Zero return value means syntax error  */
static inline int
parse_optional_fields(struct mailcap_entry *entry, char *line)
{
	ELOG
	while (0xf131d5) {
		char *field = get_mailcap_field(&line);

		if (!field) break;

		if (!c_strncasecmp(field, "needsterminal", 13)) {
			entry->needsterminal = 1;

		} else if (!c_strncasecmp(field, "copiousoutput", 13)) {
			entry->copiousoutput = 1;

		} else if (!c_strncasecmp(field, "x-htmloutput", 12)) {
			entry->x_htmloutput = 1;

		} else if (!c_strncasecmp(field, "test", 4)) {
			/* Don't leak memory if a corrupted mailcap
			 * file has multiple test commands in the same
			 * line.  */
			mem_free_set(&entry->testcommand,
				     get_mailcap_field_text(field + 4));
			if (!entry->testcommand)
				return 0;

			/* Find out wether testing requires filename */
			for (field = entry->testcommand; *field; field++)
				if (*field == '%' && *(field+1) == 's') {
					mem_free(entry->testcommand);
					entry->testcommand = NULL;
					return 0;
				}

		} else if (!c_strncasecmp(field, "description", 11)) {
			mem_free_set(&entry->description,
				     get_mailcap_field_text(field + 11));
			if (!entry->description)
				return 0;
		}
	}

	return 1;
}

/* Parses whole mailcap files line-by-line adding entries to the map
 * assigning them the given @priority */
static void
parse_mailcap_file(char *filename, unsigned int priority)
{
	ELOG
	FILE *file = fopen(filename, "rb");
	char *line = NULL;
	size_t linelen = MAX_STR_LEN;
	int lineno = 1;

	if (!file) return;

	while ((line = file_read_line(line, &linelen, file, &lineno))) {
		struct mailcap_entry *entry;
		char *linepos;
		char *command;
		char *basetypeend;
		char *type;
		int typelen;

		/* Ignore comments */
		if (*line == '#') continue;

		linepos = line;

		/* Get type */
		type = get_mailcap_field(&linepos);
		if (!type) continue;

		/* Next field is the viewcommand */
		command = get_mailcap_field(&linepos);
		if (!command) continue;

		entry = init_mailcap_entry(command, priority);
		if (!entry) continue;

		if (!parse_optional_fields(entry, linepos)) {
			done_mailcap_entry(entry);
			usrerror(gettext("Badly formatted mailcap entry "
			         "for type %s in \"%s\" line %d"),
			         type, filename, lineno);
			continue;
		}

		basetypeend = strchr(type, '/');
		typelen = strlen(type);

		if (!basetypeend) {
			char implicitwild[64];

			if (typelen + 3 > sizeof(implicitwild)) {
				done_mailcap_entry(entry);
				continue;
			}

			memcpy(implicitwild, type, typelen);
			implicitwild[typelen++] = '/';
			implicitwild[typelen++] = '*';
			implicitwild[typelen++] = '\0';
			add_mailcap_entry(entry, implicitwild, typelen);
			continue;
		}

		add_mailcap_entry(entry, type, typelen);
	}

	fclose(file);
	mem_free_if(line); /* Alloced by file_read_line() */
}


/* When initializing the mailcap map/hash read, parse and build a hash mapping
 * content type to handlers. Map is built from a list of mailcap files.
 *
 * The RFC1524 specifies that a path of mailcap files should be used.
 *	o First we check to see if the user supplied any in mime.mailcap.path
 *	o Then we check the MAILCAP environment variable.
 *	o Finally fall back to reasonable default
 */

static struct hash *
init_mailcap_map(void)
{
	ELOG
	char *path;
	unsigned int priority = 0;

	mailcap_map = init_hash8();
	if (!mailcap_map) return NULL;

	/* Try to setup mailcap_path */
	path = (char *)get_mailcap_path();
	if (!path || !*path) path = getenv("MAILCAP");
	if (!path) path = (char *)DEFAULT_MAILCAP_PATH;

	while (*path) {
		char *filename = get_next_path_filename(&path, ':');

		if (!filename) continue;
		parse_mailcap_file(filename, priority++);
		mem_free(filename);
	}

	return mailcap_map;
}

static void
done_mailcap(struct module *module)
{
	ELOG
	struct hash_item *item;
	int i;

	if (!mailcap_map) return;

	foreach_hash_item (item, *mailcap_map, i) {
		struct mailcap_hash_item *mitem = (struct mailcap_hash_item *)item->value;

		if (!mitem) continue;

		while (!list_empty(mitem->entries)) {
			struct mailcap_entry *entry = (struct mailcap_entry *)mitem->entries.next;

			del_from_list(entry);
			done_mailcap_entry(entry);
		}

		mem_free(mitem);
	}

	free_hash(&mailcap_map);
}

#ifndef TEST_MAILCAP

static int
change_hook_mailcap(struct session *ses, struct option *current, struct option *changed)
{
	ELOG
	if (changed == &get_opt_mailcap(MAILCAP_PATH)
	    || (changed == &get_opt_mailcap(MAILCAP_ENABLE)
		&& !get_mailcap_enable())) {
		done_mailcap(&mailcap_mime_module);
	}

	return 0;
}

static void
init_mailcap(struct module *module)
{
	ELOG
	static const struct change_hook_info mimetypes_change_hooks[] = {
		{ "mime.mailcap",		change_hook_mailcap },
		{ NULL,				NULL },
	};

	register_change_hooks(mimetypes_change_hooks);

	if (get_cmd_opt_bool("anonymous"))
		get_mailcap_enable() = 0;
}

#else
#define init_mailcap NULL
#endif /* TEST_MAILCAP */

/* The command semantics include the following:
 *
 * %s		is the filename that contains the mail body data
 * %t		is the content type, like text/plain
 * %{parameter} is replaced by the parameter value from the content-type
 *		field
 * \%		is %
 *
 * Unsupported RFC1524 parameters: these would probably require some doing
 * by Mutt, and can probably just be done by piping the message to metamail:
 *
 * %n		is the integer number of sub-parts in the multipart
 * %F		is "content-type filename" repeated for each sub-part
 * Only % is supported by subst_file() which is equivalent to %s. */

/* The formatting is postponed until the command is needed. This means
 * @type can be NULL. If '%t' is used in command we bail out. */
static char *
format_command(char *command, char *type, int copiousoutput, int x_htmloutput)
{
	ELOG
	struct string cmd;

	if (!init_string(&cmd)) return NULL;

	while (*command) {
		char *start = command;

		while (*command && *command != '%' && *command != '\\' && *command != '\'')
			command++;

		if (start < command)
			add_bytes_to_string(&cmd, start, command - start);

		switch (*command) {
		case '\'': /* Debian's '%s' */
			command++;
			if (!strncmp(command, "%s'", 3)) {
				command += 3;
				add_to_string(&cmd, "%f");
			} else {
				add_char_to_string(&cmd, '\'');
			}
			break;

		case '%':
			command++;
			if (!*command) {
				done_string(&cmd);
				return NULL;

			} else if (*command == 's') {
				add_to_string(&cmd, "%f");

			} else if (*command == 't') {
				if (!type) {
					done_string(&cmd);
					return NULL;
				}

				add_to_string(&cmd, type);
			}
			command++;
			break;

		case '\\':
			command++;
			if (*command) {
				add_char_to_string(&cmd, *command);
				command++;
			}
		default:
			break;
		}
	}
	return cmd.source;
}

/* Returns first usable mailcap_entry from a list where @entry is the head.
 * Use of @filename is not supported (yet). */
static struct mailcap_entry *
check_entries(struct mailcap_hash_item *item)
{
	ELOG
	struct mailcap_entry *entry;

	foreach (entry, item->entries) {
		char *test;

		/* Accept current if no test is needed */
		if (!entry->testcommand)
			return entry;

		/* We have to run the test command */
		test = format_command(entry->testcommand, NULL, 0, 0);
		if (test) {
			int exitcode = exe(test);

			mem_free(test);
			if (!exitcode)
				return entry;
		}
	}

	return NULL;
}

/* Attempts to find the given type in the mailcap association map.  On success,
 * this returns the associated command, else NULL.  Type is a string with
 * syntax '<base>/<type>' (ex: 'text/plain')
 *
 * First the given type is looked up. Then the given <base>-type with added
 * wildcard '*' (ex: 'text/<star>'). For each lookup all the associated
 * entries are checked/tested.
 *
 * The lookup supports testing on files. If no file is given (NULL) any tests
 * that need a file will be taken as failed. */

static struct mailcap_entry *
get_mailcap_entry(char *type)
{
	ELOG
	struct mailcap_entry *entry;
	struct hash_item *item;

	item = get_hash_item(mailcap_map, type, strlen(type));

	/* Check list of entries */
	entry = ((item && item->value) ? check_entries((struct mailcap_hash_item *)item->value) : NULL);

	if (!entry || get_mailcap_prioritize()) {
		/* The type lookup has either failed or we need to check
		 * the priorities so get the wild card handler */
		struct mailcap_entry *wildcard = NULL;
		char *wildpos = strchr(type, '/');

		if (wildpos) {
			int wildlen = wildpos - type + 1; /* include '/' */
			char *wildtype = memacpy(type, wildlen + 2);

			if (!wildtype) return NULL;

			wildtype[wildlen++] = '*';
			wildtype[wildlen] = '\0';

			item = get_hash_item(mailcap_map, wildtype, wildlen);
			mem_free(wildtype);

			if (item && item->value)
				wildcard = check_entries((struct mailcap_hash_item *)item->value);
		}

		/* Use @wildcard if its priority is better or @entry is NULL */
		if (wildcard && (!entry || (wildcard->priority < entry->priority)))
			entry = wildcard;
	}

	return entry;
}

#if defined(HAVE_SETENV) || defined(HAVE_PUTENV)
/** Set or unset the DISPLAY environment variable before a mailcap
 * check, or restore it afterwards.
 *
 * In a mailcap file, each entry can specify a test command that
 * checks whether the entry is applicable to the user's environment.
 * For example:
 *
 * @verbatim
 * audio/mpegurl; xmms %s; test=test "$DISPLAY" != ""
 * @endverbatim
 *
 * This means the entry should be used only if the DISPLAY environment
 * variable is not empty, i.e. there is an X display.  In ELinks,
 * check_entries() runs these test commands, so they inherit the
 * environment variables of the master ELinks process.  However, if
 * the user is running ELinks on multiple terminals, then each slave
 * ELinks process has its own environment variables, which may or may
 * not include DISPLAY.  Because the actual mailcap command may be run
 * from a slave ELinks process and inherit the environment from it,
 * any test command should also be run in the same environment.
 *
 * This function does not fully implement the ideal described above.
 * Instead, it only sets the DISPLAY environment variable as ":0" if
 * the terminal has any X display at all, or unsets DISPLAY if not.
 * This should be enough for most test commands seen in practice.
 * After the test commands of mailcap entries have been run, this
 * function must be called again to restore DISPLAY.
 *
 * @todo Retrieve all environment variables from the slave process and
 *   propagate them to the test commands.  Actually, it might be best
 *   to fork the test commands from the slave process, so that they
 *   would also inherit the controlling tty.  However, that would
 *   require changing the interlink protocol and might risk deadlocks
 *   or memory leaks if a slave terminates without responding.
 *
 * @param xwin
 *   Whether the terminal has an associated X display.
 * @param restore
 *   If this is 0, the function sets or clears DISPLAY, as described above.
 *   If this is 1, the function restores the original value of DISPLAY.
 *   There is only room for one saved value; do not nest calls.  */
static void
set_display(int xwin, int restore)
{
	ELOG
	static char *display = NULL;

	if (!restore) {
		assert(display == NULL);
		if_assert_failed mem_free(display);

		display = getenv("DISPLAY");
		if (display) display = stracpy(display);
		if (xwin) {
#ifdef HAVE_SETENV
			setenv("DISPLAY", ":0", 1);
#else
			putenv("DISPLAY=:0");
#endif
		} else {
#ifdef HAVE_UNSETENV
			unsetenv("DISPLAY");
#else
			putenv("DISPLAY");
#endif
		}
	} else { /* restore DISPLAY */
		if (display) {
#ifdef HAVE_SETENV
			setenv("DISPLAY", display, 1);
#else
			{
				static char DISPLAY[1024] = "DISPLAY=";

				/* DISPLAY[1023] == '\0' and we don't let
				 * strncpy write that far, so putenv always
				 * gets a null-terminated string.  */
				strncpy(DISPLAY + 8, display, 1023 - 8);
				putenv(DISPLAY);
			}
#endif
			mem_free_set(&display, NULL);
		} else {
#ifdef HAVE_UNSETENV
			unsetenv("DISPLAY");
#else
			putenv("DISPLAY");
#endif
		}
	}
}
#endif

static struct mime_handler *
get_mime_handler_mailcap(char *type, int xwin)
{
	ELOG
	struct mailcap_entry *entry;
	struct mime_handler *handler;
	char *program;
	int block;

	if (!get_mailcap_enable()
	    || (!mailcap_map && !init_mailcap_map()))
		return NULL;

#if defined(HAVE_SETENV) || defined(HAVE_PUTENV)
	set_display(xwin, 0);
#endif
	entry = get_mailcap_entry(type);

#if defined(HAVE_SETENV) || defined(HAVE_PUTENV)
	set_display(xwin, 1);
#endif
	if (!entry) return NULL;

	program = format_command(entry->command, type, entry->copiousoutput, entry->x_htmloutput);
	if (!program) return NULL;

	block = (entry->needsterminal || entry->copiousoutput || entry->x_htmloutput);
	handler = init_mime_handler(program, entry->description,
				    mailcap_mime_module.name,
				    get_mailcap_ask(), block);
	mem_free(program);

	handler->copiousoutput = entry->copiousoutput;
	handler->x_htmloutput = entry->x_htmloutput;
	return handler;
}


const struct mime_backend mailcap_mime_backend = {
	/* get_content_type: */	NULL,
	/* get_mime_handler: */	get_mime_handler_mailcap,
};

/* Setup the exported module. */
struct module mailcap_mime_module = struct_module(
	/* name: */		N_("Mailcap"),
	/* options: */		mailcap_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_mailcap,
	/* done: */		done_mailcap,
	/* getname: */	NULL
);

#ifdef TEST_MAILCAP

#include "util/test.h"

struct option *cmdline_options;

#ifdef CONFIG_DEBUG
union option_value *
get_opt_(char *a, int, enum option_type b, struct option *c, const char *d, struct session *e)
{
	ELOG
	return NULL;
}
#else
union option_value *
get_opt_(struct option *a , const char *b, struct session *c)
{
	ELOG
	return NULL;
}
#endif

char *get_ui_clipboard_file(void) { return NULL; }

/* Some ugly shortcuts for getting defined symbols to work. */
int default_mime_backend,
    mimetypes_mime_backend,
    program;
LIST_OF(struct terminal) terminals;

void
install_signal_handler(int sig, void (*fn)(void *), void *data, int critical)
{
}

int
main(int argc, char *argv[])
{
	ELOG
	char *format = "description,ask,block,program";
	int has_gotten = 0;
	int setenv_display = 0;
	int i;

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (strncmp(arg, "--", 2)) {
			break;
		}

		arg += 2;

		if (get_test_opt(&arg, "path", &i, argc, argv, "a string")) {
			get_mailcap_path() = arg;
			done_mailcap(NULL);

		} else if (get_test_opt(&arg, "format", &i, argc, argv, "a string")) {
			format = arg;

		} else if (get_test_opt(&arg, "display", &i, argc, argv, "a string")) {
			if (strcmp(arg, "1") == 0 || strcmp(arg, "yes") == 0) {
				setenv_display = 1;
			}

		} else if (get_test_opt(&arg, "get", &i, argc, argv, "a string")) {
			struct mime_handler *handler;

			if (has_gotten) {
				printf("\n");
			}
			has_gotten = 1;
			printf("type: %s\n", arg);
			handler = get_mime_handler_mailcap(arg, setenv_display);

			if (!handler) {
				continue;
			}

			if (strstr(format, "description")) {
				printf("description: %s\n", handler->description);
			}

			if (strstr(format, "ask")) {
				printf("ask: %d\n", handler->ask);
			}

			if (strstr(format, "block")) {
				printf("block: %d\n", handler->block);
			}

			if (strstr(format, "program")) {
				printf("program: %s\n", handler->program);
			}

		} else {
			die("Unknown argument '%s'", arg - 2);
		}
	}

	done_mailcap(NULL);

	return 0;
}

#endif /* TEST_MAILCAP */
