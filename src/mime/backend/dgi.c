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
#include "mime/backend/dgi.h"
#include "mime/mime.h"
#include "osdep/osdep.h"		/* For exe() */
#include "session/session.h"
#include "util/file.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"

struct dgi_hash_item {
	/* The entries associated with the type */
	LIST_OF(struct dgi_entry) entries;

	/* The content type of all @entries. Must be last! */
	char type[1];
};

struct dgi_entry {
	LIST_HEAD_EL(struct dgi_entry);

	char *type;
	char *inpext;
	char *outext;

	/* The 'raw' unformatted (view)command from the dgi files. */
	char command[1];
};

/* State variables */
static struct hash *dgi_map = NULL;

enum dgi_option {
	DGI_TREE,

	DGI_ENABLE,
	DGI_MIME_CFG,
	DGI_ASK
};

static union option_info dgi_options[] = {
	INIT_OPT_TREE("mime", N_("DGI"),
		"dgi", OPT_ZERO,
		N_("Dos gateway interface specific options.")),

	INIT_OPT_BOOL("mime.dgi", N_("Enable"),
		"enable", OPT_ZERO, 1,
		N_("Enable DGI support.")),

	INIT_OPT_STRING("mime.dgi", N_("Config filename"),
		"mime_cfg", OPT_ZERO, "mime.cfg",
		N_("Filename and location of config file for DGI.")),

	INIT_OPT_BOOL("mime.dgi", N_("Ask before opening"),
		"ask", OPT_ZERO, 0,
		N_("Ask before using the handlers defined by DGI.")),

	INIT_OPT_STRING("mime.dgi", N_("Path $a"),
		"a", OPT_ZERO, "",
		N_("Path to cache.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $b"),
		"b", OPT_ZERO, "",
		N_("Full name of bookmarks.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $c"),
		"c", OPT_ZERO, "",
		N_("Full name of cache index.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $d"),
		"d", OPT_ZERO, "",
		N_("Document name.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $e"),
		"e", OPT_ZERO, "",
		N_("Path to executable files.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $f"),
		"f", OPT_ZERO, "",
		N_("File browser arguments.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $g"),
		"g", OPT_ZERO, "",
		N_("IP address of 1st gateway.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $h"),
		"h", OPT_ZERO, "",
		N_("Full name of History file.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $i"),
		"i", OPT_ZERO, "",
		N_("Your IP address.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $j"),
		"j", OPT_ZERO, "",
		N_("DJPEG arguments.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $l"),
		"l", OPT_ZERO, "",
		N_("Last visited document.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $m"),
		"m", OPT_ZERO, "",
		N_("Path to mail.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $n"),
		"n", OPT_ZERO, "",
		N_("IP address of 1st nameserver.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $p"),
		"p", OPT_ZERO, "",
		N_("Host.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $q"),
		"q", OPT_ZERO, "",
		N_("Filename of query string (file created only "
		"when using this macro).")),
	INIT_OPT_STRING("mime.dgi", N_("Path $r"),
		"r", OPT_ZERO, "",
		N_("Horizontal resolution of screen.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $s"),
		"s", OPT_ZERO, "",
		N_("CGI compatible query string.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $t"),
		"t", OPT_ZERO, "",
		N_("Path for temporary files.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $u"),
		"u", OPT_ZERO, "",
		N_("URL of document.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $w"),
		"w", OPT_ZERO, "",
		N_("Download path.")),
	INIT_OPT_STRING("mime.dgi", N_("Path $x"),
		"x", OPT_ZERO, "",
		N_("Netmask.")),
	NULL_OPTION_INFO,
};

#define get_opt_dgi(which)		dgi_options[(which)].option
#define get_dgi(which)		get_opt_dgi(which).value
#define get_dgi_ask()		get_dgi(DGI_ASK).number
#define get_dgi_enable()		get_dgi(DGI_ENABLE).number

static inline void
done_dgi_entry(struct dgi_entry *entry)
{
	ELOG
	if (!entry) return;
	mem_free_if(entry->type);
	mem_free_if(entry->inpext);
	mem_free_if(entry->outext);
	mem_free(entry);
}

/* Takes care of all initialization of dgi entries.
 * Clear memory to make freeing it safer later and we get. */
static inline struct dgi_entry *
init_dgi_entry(char *type, char *inpext, char *outext, char *command)
{
	ELOG
	struct dgi_entry *entry;
	int commandlen = strlen(command);

	entry = (struct dgi_entry *)mem_calloc(1, sizeof(*entry) + commandlen);
	if (!entry) return NULL;

	memcpy(entry->command, command, commandlen);
	entry->type = stracpy(type);

	if (inpext) {
		entry->inpext = straconcat(".", inpext, NULL);
	}

	if (outext) {
		entry->outext = straconcat(".", outext, NULL);
	}

	return entry;
}

static inline void
add_dgi_entry(struct dgi_entry *entry, char *type, int typelen)
{
	ELOG
	struct dgi_hash_item *mitem;
	struct hash_item *item;

	/* Time to get the entry into the dgi_map */
	/* First check if the type is already checked in */
	item = get_hash_item(dgi_map, type, typelen);
	if (!item) {
		mitem = (struct dgi_hash_item *)mem_alloc(sizeof(*mitem) + typelen);
		if (!mitem) {
			done_dgi_entry(entry);
			return;
		}

		safe_strncpy(mitem->type, type, typelen + 1);

		init_list(mitem->entries);

		item = add_hash_item(dgi_map, mitem->type, typelen, mitem);
		if (!item) {
			mem_free(mitem);
			done_dgi_entry(entry);
			return;
		}
	} else if (item->value) {
		mitem = (struct dgi_hash_item *)item->value;
	} else {
		done_dgi_entry(entry);
		return;
	}

	add_to_list_end(mitem->entries, entry);
}

/* Parses whole mime_cfg file line-by-line adding entries to the map */
static void
parse_dgi_file(char *filename)
{
	ELOG
	FILE *file = fopen(filename, "rb");
	char *line = NULL;
	size_t linelen = MAX_STR_LEN;
	int lineno = 1;

	if (!file) return;

	while ((line = file_read_line(line, &linelen, file, &lineno))) {
		struct dgi_entry *entry;
		char *linepos;
		char *command;
		char *basetypeend;
		char *type;
		char *inpext, *outext;
		char *pipe, *greater, *space;
		int typelen;

		/* Ignore comments */
		if (*line == ';' || *line == '[') continue;

		linepos = line;

		pipe = strchr(linepos, '|');
		if (!pipe) continue;

		*pipe = '\0';
		command = pipe + 1;

		greater = strchr(linepos, '>');

		if (!greater) {
			outext = NULL;
		} else {
			*greater = '\0';
			outext = greater + 1;
		}

		space = strrchr(linepos, ' ');
		if (!space) {
			inpext = NULL;
		} else {
			inpext = space + 1;
		}

		space = strchr(linepos, ' ');
		if (!space) continue;
		*space = '\0';

		type = linepos;
		if (!*type) continue;

		entry = init_dgi_entry(type, inpext, outext, command);
		if (!entry) continue;

		basetypeend = strchr(type, '/');
		typelen = strlen(type);

		if (!basetypeend) {
			char implicitwild[64];

			if (typelen + 3 > sizeof(implicitwild)) {
				done_dgi_entry(entry);
				continue;
			}

			memcpy(implicitwild, type, typelen);
			implicitwild[typelen++] = '/';
			implicitwild[typelen++] = '*';
			implicitwild[typelen++] = '\0';
			add_dgi_entry(entry, implicitwild, typelen);
			continue;
		}

		add_dgi_entry(entry, type, typelen);
	}

	fclose(file);
	mem_free_if(line); /* Alloced by file_read_line() */
}

static struct hash *
init_dgi_map(void)
{
	ELOG
	dgi_map = init_hash8();

	if (!dgi_map) return NULL;

	parse_dgi_file(get_opt_str("mime.dgi.mime_cfg", NULL));

	return dgi_map;
}

static void
done_dgi(struct module *module)
{
	ELOG
	struct hash_item *item;
	int i;

	if (!dgi_map) return;

	foreach_hash_item (item, *dgi_map, i) {
		struct dgi_hash_item *mitem = (struct dgi_hash_item *)item->value;

		if (!mitem) continue;

		while (!list_empty(mitem->entries)) {
			struct dgi_entry *entry = (struct dgi_entry *)mitem->entries.next;

			del_from_list(entry);
			done_dgi_entry(entry);
		}

		mem_free(mitem);
	}

	free_hash(&dgi_map);
}

static int
change_hook_dgi(struct session *ses, struct option *current, struct option *changed)
{
	ELOG
	if (changed == &get_opt_dgi(DGI_MIME_CFG)
	    || (changed == &get_opt_dgi(DGI_ENABLE)
		&& !get_dgi_enable())) {
		done_dgi(&dgi_mime_module);
	}

	return 0;
}

static void
init_dgi(struct module *module)
{
	ELOG
	static const struct change_hook_info mimetypes_change_hooks[] = {
		{ "mime.dgi.enable",	change_hook_dgi },
		{ "mime.dgi.mime_cfg",	change_hook_dgi },
		{ NULL,	NULL },
	};

	register_change_hooks(mimetypes_change_hooks);

	if (get_cmd_opt_bool("anonymous"))
		get_opt_bool("mime.dgi.enable", NULL) = 0;
}

/* Returns first usable dgi_entry from a list where @entry is the head.
 * Use of @filename is not supported (yet). */
static struct dgi_entry *
check_entries(struct dgi_hash_item *item)
{
	ELOG
	struct dgi_entry *entry;

	foreach (entry, item->entries) {
		return entry;
	}

	return NULL;
}

static struct dgi_entry *
get_dgi_entry(char *type)
{
	ELOG
	struct dgi_entry *entry;
	struct hash_item *item;

	item = get_hash_item(dgi_map, type, strlen(type));

	/* Check list of entries */
	entry = ((item && item->value) ? check_entries((struct dgi_hash_item *)item->value) : NULL);

	if (!entry) {
		struct dgi_entry *wildcard = NULL;
		char *wildpos = strchr(type, '/');

		if (wildpos) {
			int wildlen = wildpos - type + 1; /* include '/' */
			char *wildtype = memacpy(type, wildlen + 2);

			if (!wildtype) return NULL;

			wildtype[wildlen++] = '*';
			wildtype[wildlen] = '\0';

			item = get_hash_item(dgi_map, wildtype, wildlen);
			mem_free(wildtype);

			if (item && item->value)
				wildcard = check_entries((struct dgi_hash_item *)item->value);
		}

		/* Use @wildcard if its priority is better or @entry is NULL */
		if (wildcard && (!entry))
			entry = wildcard;
	}

	return entry;
}

struct mime_handler *
get_mime_handler_dgi(char *type, int xwin)
{
	ELOG
	struct dgi_entry *entry;
	struct mime_handler *handler;
	char *program;

	if (!get_dgi_enable()
	    || (!dgi_map && !init_dgi_map()))
		return NULL;

	entry = get_dgi_entry(type);
	if (!entry) return NULL;

	program = stracpy(entry->command);
	if (!program) return NULL;

	handler = init_mime_handler(program, NULL,
				    dgi_mime_module.name,
				    get_dgi_ask(), 0);
	mem_free(program);

	handler->inpext = entry->inpext;
	handler->outext = entry->outext;
	handler->dgi = 1;
	return handler;
}

const struct mime_backend dgi_mime_backend = {
	/* get_content_type: */	NULL,
	/* get_mime_handler: */	get_mime_handler_dgi,
};

/* Setup the exported module. */
struct module dgi_mime_module = struct_module(
	/* name: */		N_("DGI mime"),
	/* options: */		dgi_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_dgi,
	/* done: */		done_dgi,
	/* getname: */	NULL
);
