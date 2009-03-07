/* Command line processing */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

/* We need to have it here. Stupid BSD. */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "config/cmdline.h"
#include "config/conf.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/gettext/libintl.h"
#include "network/dns.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "util/error.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


/* Hack to handle URL extraction for -remote commands */
static unsigned char *remote_url;

static enum retval
parse_options_(int argc, unsigned char *argv[], struct option *opt,
               LIST_OF(struct string_list_item) *url_list)
{
	while (argc) {
		argv++, argc--;

		if (argv[-1][0] == '-' && argv[-1][1]) {
			struct option *option;
			unsigned char *argname = &argv[-1][1];
			unsigned char *oname = stracpy(argname);
			unsigned char *err;

			if (!oname) continue;

			/* Treat --foo same as -foo. */
			if (argname[0] == '-') argname++;

			option = get_opt_rec(opt, argname);
			if (!option) option = get_opt_rec(opt, oname);
			if (!option) {
				unsigned char *pos;

				oname++; /* the '-' */
				/* Substitute '-' by '_'. This helps
				 * compatibility with that very wicked browser
				 * called 'lynx'. */
				for (pos = strchr(oname, '_'); pos;
				     pos = strchr(pos, '_'))
					*pos = '-';
				option = get_opt_rec(opt, oname);
				oname--;
			}

			mem_free(oname);

			if (!option) goto unknown_option;

			if (!option_types[option->type].cmdline)
				goto unknown_option;

			err = option_types[option->type].cmdline(option, &argv, &argc);

			if (err) {
				if (err[0]) {
					usrerror(gettext("Cannot parse option %s: %s"), argv[-1], err);

					return RET_SYNTAX;
				}

				/* XXX: Empty strings means all is well and have
				 * a cup of shut the fsck up. */
				return RET_COMMAND;

			} else if (remote_url) {
				if (url_list) add_to_string_list(url_list, remote_url, -1);
				mem_free(remote_url);
				remote_url = NULL;
			}

		} else if (url_list) {
			add_to_string_list(url_list, argv[-1], -1);
		}
	}

	return RET_OK;

unknown_option:
	usrerror(gettext("Unknown option %s"), argv[-1]);
	return RET_SYNTAX;
}

enum retval
parse_options(int argc, unsigned char *argv[],
	      LIST_OF(struct string_list_item) *url_list)
{
	return parse_options_(argc, argv, cmdline_options, url_list);
}


/**********************************************************************
 Options handlers
**********************************************************************/

static unsigned char *
eval_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	if (*argc < 1) return gettext("Parameter expected");

	(*argv)++; (*argc)--;	/* Consume next argument */

	parse_config_file(config_options, "-eval", *(*argv - 1), NULL, 0);

	fflush(stdout);

	return NULL;
}

static unsigned char *
forcehtml_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	safe_strncpy(get_opt_str("mime.default_type"), "text/html", MAX_STR_LEN);
	return NULL;
}

static unsigned char *
lookup_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct sockaddr_storage *addrs = NULL;
	int addrno, i;

	if (!*argc) return gettext("Parameter expected");
	if (*argc > 1) return gettext("Too many parameters");

	(*argv)++; (*argc)--;
	if (do_real_lookup(*(*argv - 1), &addrs, &addrno, 0) == DNS_ERROR) {
#ifdef HAVE_HERROR
		herror(gettext("error"));
#else
		usrerror(gettext("Host not found"));
#endif
		return "";
	}

	for (i = 0; i < addrno; i++) {
#ifdef CONFIG_IPV6
		struct sockaddr_in6 addr = *((struct sockaddr_in6 *) &(addrs)[i]);
		unsigned char p[INET6_ADDRSTRLEN];

		if (! inet_ntop(addr.sin6_family,
				(addr.sin6_family == AF_INET6 ? (void *) &addr.sin6_addr
							      : (void *) &((struct sockaddr_in *) &addr)->sin_addr),
				p, INET6_ADDRSTRLEN))
			ERROR(gettext("Resolver error"));
		else
			printf("%s\n", p);
#else
		struct sockaddr_in addr = *((struct sockaddr_in *) &(addrs)[i]);
		unsigned char *p = (unsigned char *) &addr.sin_addr.s_addr;

		printf("%d.%d.%d.%d\n", (int) p[0], (int) p[1],
				        (int) p[2], (int) p[3]);
#endif
	}

	mem_free_if(addrs);

	fflush(stdout);

	return "";
}

#define skipback_whitespace(start, S) \
	while ((start) < (S) && isspace((S)[-1])) (S)--;

static unsigned char *
remote_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct {
		unsigned char *name;
		enum {
			REMOTE_METHOD_OPENURL,
			REMOTE_METHOD_PING,
			REMOTE_METHOD_XFEDOCOMMAND,
			REMOTE_METHOD_ADDBOOKMARK,
			REMOTE_METHOD_INFOBOX,
			REMOTE_METHOD_NOT_SUPPORTED,
		} type;
	} remote_methods[] = {
		{ "openURL",	  REMOTE_METHOD_OPENURL },
		{ "ping",	  REMOTE_METHOD_PING },
		{ "addBookmark",  REMOTE_METHOD_ADDBOOKMARK },
		{ "infoBox",	  REMOTE_METHOD_INFOBOX },
		{ "xfeDoCommand", REMOTE_METHOD_XFEDOCOMMAND },
		{ NULL,		  REMOTE_METHOD_NOT_SUPPORTED },
	};
	unsigned char *command, *arg, *argend, *argstring;
	int method, len = 0;
	unsigned char *remote_argv[10];
	int remote_argc;

	if (*argc < 1) return gettext("Parameter expected");

	command = *(*argv);

	while (isasciialpha(command[len]))
		len++;

	/* Find the begining and end of the argument list. */

	arg = command + len;
	skip_space(arg);

	argend = arg + strlen(arg);
	skipback_whitespace(arg, argend);
	if (argend > arg)
		argend--;

	/* Decide whether to use the "extended" --remote format where
	 * all URLs following should be opened in tabs. */
	if (len == 0 || *arg != '(' || *argend != ')') {
		/* Just open any passed URLs in new tabs */
		remote_session_flags |= SES_REMOTE_NEW_TAB;
		return NULL;
	}

	arg++;

	arg = argstring = memacpy(arg, argend - arg);
	if (!argstring)
		return gettext("Out of memory");

	remote_argc = 0;
	do {
		unsigned char *start, *end;

		if (remote_argc > sizeof_array(remote_argv)) {
			mem_free(argstring);
			return gettext("Too many arguments");
		}

		/* Skip parenthesis, comma, and surrounding whitespace. */
		skip_space(arg);
		start = arg;

		if (*start == '"') {
			end = ++start;
			while ((end = strchr(end, '"'))) {
				/* Treat "" inside quoted arg as ". */
				if (end[1] != '"')
					break;

				end += 2;
			}

			if (!end)
				return gettext("Mismatched ending argument quoting");

			arg = end + 1;
			skip_space(arg);
			if (*arg && *arg != ',')
				return gettext("Garbage after quoted argument");

			remote_argv[remote_argc++] = start;
			*end = 0;

			/* Unescape "" to ". */
			for (end = start; *end; start++, end++) {
				*start = *end;
				if (*end == '"')
					end++;
			}
			*start = 0;

		} else {
			end = strchr(start, ',');
			if (!end) {
				end = start + strlen(start);
				arg = end;
			} else {
				arg = end + 1;
			}
			skipback_whitespace(start, end);

			if (start != end)
				remote_argv[remote_argc++] = start;
			*end = 0;
		}

		if (*arg == ',')
			arg++;
	} while (*arg);

	for (method = 0; remote_methods[method].name; method++) {
		unsigned char *name = remote_methods[method].name;

		if (!c_strlcasecmp(command, len, name, -1))
			break;
	}

	switch (remote_methods[method].type) {
	case REMOTE_METHOD_OPENURL:
		if (remote_argc < 1) {
			/* Prompt for a URL with a dialog box */
			remote_session_flags |= SES_REMOTE_PROMPT_URL;
			break;
		}

		if (remote_argc == 2) {
			unsigned char *where = remote_argv[1];

			if (strstr(where, "new-window")) {
				remote_session_flags |= SES_REMOTE_NEW_WINDOW;

			} else if (strstr(where, "new-tab")) {
				remote_session_flags |= SES_REMOTE_NEW_TAB;

			} else {
				/* Bail out when getting unknown parameter */
				/* TODO: new-screen */
				break;
			}

		} else {
			remote_session_flags |= SES_REMOTE_CURRENT_TAB;
		}

		remote_url = stracpy(remote_argv[0]);
		break;

	case REMOTE_METHOD_XFEDOCOMMAND:
		if (remote_argc < 1)
			break;

		if (!c_strcasecmp(remote_argv[0], "openBrowser")) {
			remote_session_flags = SES_REMOTE_NEW_WINDOW;
		}
		break;

	case REMOTE_METHOD_PING:
		remote_session_flags = SES_REMOTE_PING;
		break;

	case REMOTE_METHOD_ADDBOOKMARK:
		if (remote_argc < 1)
			break;
		remote_url = stracpy(remote_argv[0]);
		remote_session_flags = SES_REMOTE_ADD_BOOKMARK;
		break;

	case REMOTE_METHOD_INFOBOX:
		if (remote_argc < 1)
			break;
		remote_url = stracpy(remote_argv[0]);
		if (remote_url)
			insert_in_string(&remote_url, 0, "about:", 6);
		remote_session_flags = SES_REMOTE_INFO_BOX;
		break;

	case REMOTE_METHOD_NOT_SUPPORTED:
		break;
	}

	mem_free(argstring);

	/* If no flags was applied it can only mean we are dealing with
	 * unknown method. */
	if (!remote_session_flags)
		return gettext("Remote method not supported");

	(*argv)++; (*argc)--;	/* Consume next argument */

	return NULL;
}

static unsigned char *
version_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	printf("%s\n", full_static_version);
	fflush(stdout);
	return "";
}


/* Below we handle help usage printing.
 *
 * We're trying to achieve several goals here:
 *
 * - Genericly define a function to print option trees iteratively.
 * - Make output parsable for various doc tools (to make manpages).
 * - Do some non generic fancy stuff like printing semi-aliased
 *   options (like: -?, -h and -help) on one line when printing
 *   short help. */

#define gettext_nonempty(x) (*(x) ? gettext(x) : (x))

static void print_option_desc(const unsigned char *desc)
{
	struct string wrapped;
	static const struct string indent = INIT_STRING("            ", 12);

	if (init_string(&wrapped)
	    && wrap_option_desc(&wrapped, desc, &indent, 79 - indent.length)) {
		/* struct string could in principle contain null
		 * characters, so don't use printf() or fputs().  */
		fwrite(wrapped.source, 1, wrapped.length, stdout);
	} else {
		/* Write the error to stderr so it appears on the
		 * screen even if stdout is being parsed by a script
		 * that reformats it to HTML or such.  */
		fprintf(stderr, "%12s%s\n", "",
			gettext("Out of memory formatting option documentation"));
	}

	/* done_string() is safe even if init_string() failed.  */
	done_string(&wrapped);
}

static void print_full_help_outer(struct option *tree, unsigned char *path);

static void
print_full_help_inner(struct option *tree, unsigned char *path,
		      int trees)
{
	struct option *option;
	unsigned char saved[MAX_STR_LEN];
	unsigned char *savedpos = saved;

	*savedpos = 0;

	foreach (option, *tree->value.tree) {
		enum option_type type = option->type;
		unsigned char *help;
		unsigned char *capt = option->capt;
		unsigned char *desc = (option->desc && *option->desc)
				      ? (unsigned char *) gettext(option->desc)
				      : (unsigned char *) "N/A";

		if (trees != (type == OPT_TREE))
			continue;

		/* Don't print deprecated aliases (if we don't walk command
		 * line options which use aliases for legitimate options). */
		if ((type == OPT_ALIAS && tree != cmdline_options)
		    || (option->flags & OPT_HIDDEN))
			continue;

		if (!capt && !c_strncasecmp(option->name, "_template_", 10))
			capt = (unsigned char *) N_("Template option folder");

		if (!capt) {
			int len = strlen(option->name);
			int max = MAX_STR_LEN - (savedpos - saved);

			safe_strncpy(savedpos, option->name, max);
			safe_strncpy(savedpos + len, ", -", max - len);
			savedpos += len + 3;
			continue;
		}

		help = gettext_nonempty(option_types[option->type].help_str);

		if (type != OPT_TREE)
			printf("    %s%s%s %s ",
				path, saved, option->name, help);

		/* Print the 'title' of each option type. */
		switch (type) {
			case OPT_BOOL:
			case OPT_INT:
			case OPT_LONG:
				printf(gettext("(default: %ld)"),
					type == OPT_LONG
					? option->value.big_number
					: (long) option->value.number);
				break;

			case OPT_STRING:
				printf(gettext("(default: \"%s\")"),
					option->value.string);
				break;

			case OPT_ALIAS:
				printf(gettext("(alias for %s)"),
					option->value.string);
				break;

			case OPT_CODEPAGE:
				printf(gettext("(default: %s)"),
					get_cp_name(option->value.number));
				break;

			case OPT_COLOR:
			{
				color_T color = option->value.color;
				unsigned char hexcolor[8];

				printf(gettext("(default: %s)"),
				       get_color_string(color, hexcolor));
				break;
			}

			case OPT_COMMAND:
				break;

			case OPT_LANGUAGE:
#ifdef CONFIG_NLS
				printf(gettext("(default: \"%s\")"),
				       language_to_name(option->value.number));
#endif
				break;

			case OPT_TREE:
			{
				int pathlen = strlen(path);
				int namelen = strlen(option->name);

				if (pathlen + namelen + 2 > MAX_STR_LEN)
					continue;

				/* Append option name to path */
				if (pathlen > 0) {
					memcpy(saved, path, pathlen);
					savedpos = saved + pathlen;
				} else {
					savedpos = saved;
				}
				memcpy(savedpos, option->name, namelen + 1);
				savedpos += namelen;

				capt = gettext_nonempty(capt);
				printf("  %s: (%s)", capt, saved);
				break;
			}
		}

		putchar('\n');
		print_option_desc(desc);
		putchar('\n');

		if (option->type == OPT_TREE) {
			memcpy(savedpos, ".", 2);
			print_full_help_outer(option, saved);
		}

		savedpos = saved;
		*savedpos = 0;
	}
}

static void
print_full_help_outer(struct option *tree, unsigned char *path)
{
	print_full_help_inner(tree, path, 0);
	print_full_help_inner(tree, path, 1);
}

static void
print_short_help(void)
{
#define ALIGN_WIDTH 20
	struct option *option;
	struct string string = NULL_STRING;
	struct string *saved = NULL;
	unsigned char align[ALIGN_WIDTH];

	/* Initialize @space used to align captions. */
	memset(align, ' ', sizeof(align) - 1);
	align[sizeof(align) - 1] = 0;

	foreach (option, *cmdline_options->value.tree) {
		unsigned char *capt;
		unsigned char *help;
		unsigned char *info = saved ? saved->source
					    : (unsigned char *) "";
		int len = strlen(option->name);

		/* Avoid printing compatibility options */
		if (option->flags & OPT_HIDDEN)
			continue;

		/* When no caption is available the option name is 'stacked'
		 * and the caption is shared with next options that has one. */
		if (!option->capt) {
			if (!saved) {
				if (!init_string(&string))
					continue;

				saved = &string;
			}

			add_to_string(saved, option->name);
			add_to_string(saved, ", -");
			continue;
		}

		capt = gettext_nonempty(option->capt);
		help = gettext_nonempty(option_types[option->type].help_str);

		/* When @help string is non empty align at least one space. */
		len = ALIGN_WIDTH - len - strlen(help);
		len -= (saved ? saved->length : 0);
		len = (len < 0) ? !!(*help) : len;

		align[len] = '\0';
		printf("  -%s%s %s%s%s\n",
		       info, option->name, help, align, capt);
		align[len] = ' ';
		if (saved) {
			done_string(saved);
			saved = NULL;
		}
	}
#undef ALIGN_WIDTH
}

#undef gettext_nonempty

static unsigned char *
printhelp_cmd(struct option *option, unsigned char ***argv, int *argc)
{
	unsigned char *lineend = strchr(full_static_version, '\n');

	if (lineend) *lineend = '\0';

	printf("%s\n\n", full_static_version);

	if (!strcmp(option->name, "config-help")) {
		printf("%s:\n", gettext("Configuration options"));
		print_full_help_outer(config_options, "");
	} else {
		printf("%s\n\n%s:\n",
		       gettext("Usage: elinks [OPTION]... [URL]..."),
		       gettext("Options"));
		if (!strcmp(option->name, "long-help")) {
			print_full_help_outer(cmdline_options, "-");
		} else {
			print_short_help();
		}
	}

	fflush(stdout);
	return "";
}

static unsigned char *
redir_cmd(struct option *option, unsigned char ***argv, int *argc)
{
	unsigned char *target;

	/* I can't get any dirtier. --pasky */

	if (!strcmp(option->name, "confdir")) {
		target = "config-dir";
	} else if (!strcmp(option->name, "conffile")) {
		target = "config-file";
	} else if (!strcmp(option->name, "stdin")) {
		static int complained;

		if (complained)
			return NULL;
		complained = 1;

		/* Emulate bool option, possibly eating following 0/1. */
		if ((*argv)[0]
		    && ((*argv)[0][0] == '0' || (*argv)[0][0] == '1')
		    && !(*argv)[0][1])
			(*argv)++, (*argc)--;
		fprintf(stderr, "Warning: Deprecated option -stdin used!\n");
		fprintf(stderr, "ELinks now determines the -stdin option value automatically.\n");
		fprintf(stderr, "In the future versions ELinks will report error when you will\n");
		fprintf(stderr, "continue to use this option.\a\n");
		return NULL;

	} else {
		return gettext("Internal consistency error");
	}

	option = get_opt_rec(cmdline_options, target);
	assert(option);
	option_types[option->type].cmdline(option, argv, argc);
	return NULL;
}

static unsigned char *
printconfigdump_cmd(struct option *option, unsigned char ***argv, int *argc)
{
	unsigned char *config_string;

	/* Print all. */
	get_opt_int("config.saving_style") = 2;

	config_string = create_config_string("", "", config_options);
	if (config_string) {
		printf("%s", config_string);
		mem_free(config_string);
	}

	fflush(stdout);
	return "";
}


/**********************************************************************
 Options values
**********************************************************************/

/* Keep options in alphabetical order. */

struct option_info cmdline_options_info[] = {
	/* [gettext_accelerator_context(IGNORE)] */
	INIT_OPT_BOOL("", N_("Restrict to anonymous mode"),
		"anonymous", 0, 0,
		N_("Restricts ELinks so it can run on an anonymous account. "
		"Local file browsing, downloads, and modification of options "
		"will be disabled. Execution of viewers is allowed, but "
		"entries in the association table can't be added or "
		"modified.")),

	INIT_OPT_BOOL("", N_("Autosubmit first form"),
		"auto-submit", 0, 0,
		N_("Automatically submit the first form in the given URLs.")),

	INIT_OPT_INT("", N_("Clone internal session with given ID"),
		"base-session", 0, 0, INT_MAX, 0,
		N_("Used internally when opening ELinks instances in new "
		"windows. The ID maps to information that will be used when "
		"creating the new instance. You don't want to use it.")),

	INIT_OPT_COMMAND("", NULL, "confdir", OPT_HIDDEN, redir_cmd, NULL),

	INIT_OPT_STRING("", N_("Name of directory with configuration file"),
		"config-dir", 0, "",
		N_("Path of the directory ELinks will read and write its "
		"config and runtime state files to instead of ~/.elinks. "
		"If the path does not begin with a '/' it is assumed to be "
		"relative to your HOME directory.")),

	INIT_OPT_COMMAND("", N_("Print default configuration file to stdout"),
		"config-dump", 0, printconfigdump_cmd,
		N_("Print a configuration file with options set to the "
		"built-in defaults to stdout.")),

	INIT_OPT_COMMAND("", NULL, "conffile", OPT_HIDDEN, redir_cmd, NULL),

	INIT_OPT_STRING("", N_("Name of configuration file"),
		"config-file", 0, "elinks.conf",
		N_("Name of the configuration file that all configuration "
		"options will be read from and written to. It should be "
		"relative to config-dir.")),

	INIT_OPT_COMMAND("", N_("Print help for configuration options"),
		"config-help", 0, printhelp_cmd,
		N_("Print help for configuration options and exit.")),

	INIT_OPT_CMDALIAS("", N_("MIME type assumed for unknown document types"),
		"default-mime-type", 0, "mime.default_type",
		N_("The default MIME type used for documents of unknown "
		"type.")),

	INIT_OPT_BOOL("", N_("Ignore user-defined keybindings"),
		"default-keys", 0, 0,
		N_("When set, all keybindings from configuration files will "
		"be ignored. It forces use of default keybindings and will "
		"reset user-defined ones on save.")),

	INIT_OPT_BOOL("", N_("Print formatted versions of given URLs to stdout"),
		"dump", 0, 0,
		N_("Print formatted plain-text versions of given URLs to "
		"stdout.")),

	INIT_OPT_CMDALIAS("", N_("Codepage to use with -dump"),
		"dump-charset", 0, "document.dump.codepage",
		N_("Codepage used when formatting dump output.")),

	INIT_OPT_CMDALIAS("", N_("Color mode used with -dump"),
		"dump-color-mode", 0, "document.dump.color_mode",
		N_("Color mode used with -dump.")),

	INIT_OPT_CMDALIAS("", N_("Width of document formatted with -dump"),
		"dump-width", 0, "document.dump.width",
		N_("Width of the dump output.")),

	INIT_OPT_COMMAND("", N_("Evaluate configuration file directive"),
		"eval", 0, eval_cmd,
		N_("Specify configuration file directives on the command-line "
		"which will be evaluated after all configuration files has "
		"been read. Example usage:\n"
		"\t-eval 'set protocol.file.allow_special_files = 1'")),

	/* lynx compatibility */
	INIT_OPT_COMMAND("", N_("Interpret documents of unknown types as HTML"),
		"force-html", 0, forcehtml_cmd,
		N_("Makes ELinks assume documents of unknown types are HTML. "
		"Useful when using ELinks as an external viewer from MUAs. "
		"This is equivalent to -default-mime-type text/html.")),

	/* XXX: -?, -h and -help share the same caption and should be kept in
	 * the current order for usage help printing to be ok */
	INIT_OPT_COMMAND("", NULL, "?", 0, printhelp_cmd, NULL),

	INIT_OPT_COMMAND("", NULL, "h", 0, printhelp_cmd, NULL),

	INIT_OPT_COMMAND("", N_("Print usage help and exit"),
		"help", 0, printhelp_cmd,
		N_("Print usage help and exit.")),

	INIT_OPT_BOOL("", N_("Only permit local connections"),
		"localhost", 0, 0,
		N_("Restricts ELinks to work offline and only connect to "
		"servers with local addresses (ie. 127.0.0.1). No connections "
		"to remote servers will be permitted.")),

	INIT_OPT_COMMAND("", N_("Print detailed usage help and exit"),
		"long-help", 0, printhelp_cmd,
		N_("Print detailed usage help and exit.")),

	INIT_OPT_COMMAND("", N_("Look up specified host"),
		"lookup", 0, lookup_cmd,
		N_("Look up specified host and print all DNS resolved IP "
		"addresses.")),

	INIT_OPT_BOOL("", N_("Run as separate instance"),
		"no-connect", 0, 0,
		N_("Run ELinks as a separate instance instead of connecting "
		"to an existing instance. Note that normally no runtime state "
		"files (bookmarks, history, etc.) are written to the disk "
		"when this option is used. See also -touch-files.")),

	INIT_OPT_BOOL("", N_("Disable use of files in ~/.elinks"),
		"no-home", 0, 0,
		N_("Disables creation and use of files in the user specific "
		"home configuration directory (~/.elinks). It forces default "
		"configuration values to be used and disables saving of "
		"runtime state files.")),

	INIT_OPT_CMDALIAS("", N_("Disable link numbering in dump output"),
		"no-numbering", OPT_ALIAS_NEGATE, "document.dump.numbering",
		N_("Prevents printing of link number in dump output.\n"
		"\n"
		"Note that this really affects only -dump, nothing else.")),

	INIT_OPT_CMDALIAS("", N_("Disable printing of link references in dump output"),
		"no-references", OPT_ALIAS_NEGATE, "document.dump.references",
		N_("Prevents printing of references (URIs) of document links "
		"in dump output.\n"
		"\n"
		"Note that this really affects only -dump, nothing else.")),

	INIT_OPT_COMMAND("", N_("Control an already running ELinks"),
		"remote", 0, remote_cmd,
		N_("Control a remote ELinks instance by passing commands to "
		"it. The option takes an additional argument containing the "
		"method which should be invoked and any parameters that "
		"should be passed to it. For ease of use, the additional "
		"method argument can be omitted in which case any URL "
		"arguments will be opened in new tabs in the remote "
		"instance.\n"
		"\n"
		"Following is a list of the supported methods:\n"
		"\tping()                    : look for a remote instance\n"
		"\topenURL()                 : prompt URL in current tab\n"
		"\topenURL(URL)              : open URL in current tab\n"
		"\topenURL(URL, new-tab)     : open URL in new tab\n"
		"\topenURL(URL, new-window)  : open URL in new window\n"
		"\taddBookmark(URL)          : bookmark URL\n"
		"\tinfoBox(text)             : show text in a message box\n"
		"\txfeDoCommand(openBrowser) : open new window")),

	INIT_OPT_INT("", N_("Connect to session ring with given ID"),
		"session-ring", 0, 0, INT_MAX, 0,
		N_("ID of session ring this ELinks session should connect to. "
		"ELinks works in so-called session rings, whereby all "
		"instances of ELinks are interconnected and share state "
		"(cache, bookmarks, cookies, and so on). By default, all "
		"ELinks instances connect to session ring 0. You can change "
		"that behaviour with this switch and form as many session "
		"rings as you want. Obviously, if the session-ring with this "
		"number doesn't exist yet, it's created and this ELinks "
		"instance will become the master instance (that usually "
		"doesn't matter for you as a user much).\n"
		"\n"
		"Note that you usually don't want to use this unless you're a "
		"developer and you want to do some testing - if you want the "
		"ELinks instances each running standalone, rather use the "
		"-no-connect command-line option. Also note that normally no "
		"runtime state files are written to the disk when this option "
		"is used. See also -touch-files.")),

	INIT_OPT_BOOL("", N_("Print the source of given URLs to stdout"),
		"source", 0, 0,
		N_("Print given URLs in source form to stdout.")),

	INIT_OPT_COMMAND("", NULL, "stdin", OPT_HIDDEN, redir_cmd, NULL),

	INIT_OPT_BOOL("", N_("Touch files in ~/.elinks when running with -no-connect/-session-ring"),
		"touch-files", 0, 0,
		N_("When enabled, runtime state files (bookmarks, history, "
		"etc.) are written to disk, even when -no-connect or "
		"-session-ring is used. The option has no effect if not used "
		"in conjunction with any of these options.")),

	INIT_OPT_INT("", N_("Verbose level"),
		"verbose", 0, 0, VERBOSE_LEVELS - 1, VERBOSE_WARNINGS,
		N_("The verbose level controls what messages are shown at "
		"start up and while running:\n"
		"\t0 means only show serious errors\n"
		"\t1 means show serious errors and warnings\n"
		"\t2 means show all messages")),

	INIT_OPT_COMMAND("", N_("Print version information and exit"),
		"version", 0, version_cmd,
		N_("Print ELinks version information and exit.")),

	NULL_OPTION_INFO,
};
