/* Config file manipulation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/conf.h"
#include "config/dialogs.h"
#include "config/home.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/gettext/libintl.h"
#include "osdep/osdep.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"


/* Config file has only very simple grammar:
 *
 * /set *option *= *value/
 * /bind *keymap *keystroke *= *action/
 * /include *file/
 * /#.*$/
 *
 * Where option consists from any number of categories separated by dots and
 * name of the option itself. Both category and option name consists from
 * [a-zA-Z0-9_-*] - using uppercase letters is not recommended, though. '*' is
 * reserved and is used only as escape character in place of '.' originally in
 * option name.
 *
 * Value can consist from:
 * - number (it will be converted to int/long)
 * - enum (like on, off; true, fake, last_url; etc ;) - in planning state yet
 * - string - "blah blah" (keymap, keystroke and action and file looks like that too)
 *
 * "set" command is parsed first, and then type-specific function is called,
 * with option as one parameter and value as a second. Usually it just assigns
 * value to an option, but sometimes you may want to first create the option
 * ;). Then this will come handy. */

struct conf_parsing_state {
	/** This part may be copied to a local variable as a bookmark
	 * and restored later.  So it must not contain any pointers
	 * that would have to be freed in that situation.  */
	struct conf_parsing_pos {
		/** Points to the next character to be parsed from the
		 * configuration file.  */
		unsigned char *look;

		/** The line number corresponding to #look.  This is
		 * shown in error messages.  */
		int line;
	} pos;

	/** When ELinks is rewriting the configuration file, @c mirrored
	 * indicates the end of the part that has already been copied
	 * to the mirror string.  Otherwise, @c mirrored is not used.
	 *
	 * @invariant @c mirrored @<= @c pos.look */
	unsigned char *mirrored;

	/** File name for error messages.  If NULL then do not display
	 * error messages.  */
	const unsigned char *filename;
};

/** Tell the user about an error in the configuration file.
 * @return @a err, for convenience.  */
static enum parse_error
show_parse_error(const struct conf_parsing_state *state, enum parse_error err)
{
	static const unsigned char error_msg[][40] = {
		"no error",        /* ERROR_NONE */
		"unknown command", /* ERROR_COMMAND */
		"parse error",     /* ERROR_PARSE */
		"unknown option",  /* ERROR_OPTION */
		"bad value",       /* ERROR_VALUE */
		"no memory left",  /* ERROR_NOMEM */
	};

	if (state->filename) {
		fprintf(stderr, "%s:%d: %s\n",
			state->filename, state->pos.line, error_msg[err]);
	}
	return err;
}

/** Skip comments and whitespace.  */
static void
skip_white(struct conf_parsing_pos *pos)
{
	unsigned char *start = pos->look;

	while (*start) {
		while (isspace(*start)) {
			if (*start == '\n') {
				pos->line++;
			}
			start++;
		}

		if (*start == '#') {
			start += strcspn(start, "\n");
		} else {
			pos->look = start;
			return;
		}
	}

	pos->look = start;
}

/** Skip a quoted string.
 * This function allows "mismatching quotes' because str_rd() does so.  */
static void
skip_quoted(struct conf_parsing_pos *pos)
{
	assert(isquote(*pos->look));
	if_assert_failed return;
	pos->look++;

	for (;;) {
		if (!*pos->look)
			return;
		if (isquote(*pos->look)) {
			pos->look++;
			return;
		}
		if (*pos->look == '\\' && pos->look[1])
			pos->look++;
		if (*pos->look == '\n')
			pos->line++;
		pos->look++;
	}
}

/** Skip the value of an option.
 *
 * This job is normally done by the reader function that corresponds
 * to the type of the option.  However, if ELinks does not recognize
 * the name of the option, it cannot look up the type and has to use
 * this function instead.  */
static void
skip_option_value(struct conf_parsing_pos *pos)
{
	if (isquote(*pos->look)) {
		/* Looks like OPT_STRING, OPT_CODEPAGE, OPT_LANGUAGE,
		 * or OPT_COLOR.  */
		skip_quoted(pos);
	} else {
		/* Looks like OPT_BOOL, OPT_INT, or OPT_LONG.  */
		while (isasciialnum(*pos->look) || *pos->look == '.'
		       || *pos->look == '+' || *pos->look == '-')
			pos->look++;
	}
}

/** Skip to the next newline or comment that is not part of a quoted
 * string.  When ELinks hits a parse error in the configuration file,
 * it calls this in order to find the place where is should resume
 * parsing.  This is intended to prevent ELinks from treating words
 * in strings as commands.  */
static void
skip_to_unquoted_newline_or_comment(struct conf_parsing_pos *pos)
{
	while (*pos->look && *pos->look != '#' && *pos->look != '\n') {
		if (isquote(*pos->look))
			skip_quoted(pos);
		else
			pos->look++;
	}
}

/* Parse a command. Returns error code. */
/* If dynamic string credentials are supplied, we will mirror the command at
 * the end of the string; however, we won't load the option value to the tree,
 * and we will even write option value from the tree to the output string.
 * We will only possibly set or clear OPT_MUST_SAVE flag in the option.  */

static enum parse_error
parse_set(struct option *opt_tree, struct conf_parsing_state *state,
	  struct string *mirror, int is_system_conf)
{
	const unsigned char *optname_orig;
	size_t optname_len;
	unsigned char *optname_copy;

	skip_white(&state->pos);
	if (!*state->pos.look) return show_parse_error(state, ERROR_PARSE);

	/* Option name */
	optname_orig = state->pos.look;
	while (is_option_name_char(*state->pos.look)
	       || *state->pos.look == '.')
		state->pos.look++;
	optname_len = state->pos.look - optname_orig;

	skip_white(&state->pos);

	/* Equal sign */
	if (*state->pos.look != '=')
		return show_parse_error(state, ERROR_PARSE);
	state->pos.look++; /* '=' */
	skip_white(&state->pos);
	if (!*state->pos.look)
		return show_parse_error(state, ERROR_VALUE);

	optname_copy = memacpy(optname_orig, optname_len);
	if (!optname_copy) return show_parse_error(state, ERROR_NOMEM);

	/* Option value */
	{
		struct option *opt;
		unsigned char *val;
		const struct conf_parsing_pos pos_before_value = state->pos;

		opt = mirror
			? get_opt_rec_real(opt_tree, optname_copy)
			: get_opt_rec(opt_tree, optname_copy);
		mem_free(optname_copy);
		optname_copy = NULL;

		if (!opt || (opt->flags & OPT_HIDDEN)) {
			show_parse_error(state, ERROR_OPTION);
			skip_option_value(&state->pos);
			return ERROR_OPTION;
			/* TODO: Distinguish between two scenarios:
			 * - A newer version of ELinks has saved an
			 *   option that this version does not recognize.
			 *   The option must be preserved.  (This works.)
			 * - The user has added an option, saved
			 *   elinks.conf, restarted ELinks, deleted the
			 *   option, and is now saving elinks.conf again.
			 *   The option should be rewritten to "unset".
			 *   (This does not work yet.)
			 * In both cases, ELinks has no struct option
			 * for that name.  Possible fixes:
			 * - If the tree has OPT_AUTOCREATE, then
			 *   assume the user had created that option,
			 *   and rewrite it to "unset".  Otherwise,
			 *   keep it.
			 * - When the user deletes an option, just mark
			 *   it with OPT_DELETED, and keep it in memory
			 *   as long as OPT_TOUCHED is set.  */
		}

		if (!option_types[opt->type].read) {
			show_parse_error(state, ERROR_VALUE);
			skip_option_value(&state->pos);
			return ERROR_VALUE;
		}

		val = option_types[opt->type].read(opt, &state->pos.look,
						   &state->pos.line);
		if (!val) {
			/* The reader function failed.  Jump back to
			 * the beginning of the value and skip it with
			 * the generic code.  For the error message,
			 * use the line number at the beginning of the
			 * value, because the ending position is not
			 * interesting if there is an unclosed quote.  */
			state->pos = pos_before_value;
			show_parse_error(state, ERROR_VALUE);
			skip_option_value(&state->pos);
			return ERROR_VALUE;
		}

		if (!mirror) {
			/* loading a configuration file */
			if (!option_types[opt->type].set
			    || !option_types[opt->type].set(opt, val)) {
				mem_free(val);
				return show_parse_error(state, ERROR_VALUE);
			}
		} else if (is_system_conf) {
			/* scanning a file that will not be rewritten */
			struct option *flagsite = indirect_option(opt);

			if (!(flagsite->flags & OPT_DELETED)
			    && option_types[opt->type].equals
			    && option_types[opt->type].equals(opt, val))
				flagsite->flags &= ~OPT_MUST_SAVE;
			else
				flagsite->flags |= OPT_MUST_SAVE;
		} else {
			/* rewriting a configuration file */
			struct option *flagsite = indirect_option(opt);

			if (flagsite->flags & OPT_DELETED) {
				/* Replace the "set" command with an
				 * "unset" command.  */
				add_to_string(mirror, "unset ");
				add_bytes_to_string(mirror, optname_orig,
						    optname_len);
				state->mirrored = state->pos.look;
			} else if (option_types[opt->type].write) {
				add_bytes_to_string(mirror, state->mirrored,
						    pos_before_value.look
						    - state->mirrored);
				option_types[opt->type].write(opt, mirror);
				state->mirrored = state->pos.look;
			}
			/* Remember that the option need not be
			 * written to the end of the file.  */
			flagsite->flags &= ~OPT_MUST_SAVE;
		}
		mem_free(val);
	}

	return ERROR_NONE;
}

static enum parse_error
parse_unset(struct option *opt_tree, struct conf_parsing_state *state,
	    struct string *mirror, int is_system_conf)
{
	const unsigned char *optname_orig;
	size_t optname_len;
	unsigned char *optname_copy;

	skip_white(&state->pos);
	if (!*state->pos.look) return show_parse_error(state, ERROR_PARSE);

	/* Option name */
	optname_orig = state->pos.look;
	while (is_option_name_char(*state->pos.look)
	       || *state->pos.look == '.')
		state->pos.look++;
	optname_len = state->pos.look - optname_orig;

	optname_copy = memacpy(optname_orig, optname_len);
	if (!optname_copy) return show_parse_error(state, ERROR_NOMEM);

	{
		struct option *opt;

		opt = get_opt_rec_real(opt_tree, optname_copy);
		mem_free(optname_copy);
		optname_copy = NULL;

		if (!opt || (opt->flags & OPT_HIDDEN)) {
			/* The user wanted to delete the option, and
			 * it has already been deleted; this is not an
			 * error.  This might happen if a version of
			 * ELinks has a built-in URL rewriting rule,
			 * the user disables it, and a later version
			 * no longer has it.  */
			return ERROR_NONE;
		}

		if (!mirror) {
			/* loading a configuration file */
			if (opt->flags & OPT_ALLOC) delete_option(opt);
			else mark_option_as_deleted(opt);
		} else if (is_system_conf) {
			/* scanning a file that will not be rewritten */
			struct option *flagsite = indirect_option(opt);

			if (flagsite->flags & OPT_DELETED)
				flagsite->flags &= ~OPT_MUST_SAVE;
			else
				flagsite->flags |= OPT_MUST_SAVE;
		} else {
			/* rewriting a configuration file */
			struct option *flagsite = indirect_option(opt);

			if (flagsite->flags & OPT_DELETED) {
				/* The "unset" command is already in the file,
				 * and unlike with "set", there is no value
				 * to be updated.  */
			} else if (option_types[opt->type].write) {
				/* Replace the "unset" command with a
				 * "set" command.  */
				add_to_string(mirror, "set ");
				add_bytes_to_string(mirror, optname_orig,
						    optname_len);
				add_to_string(mirror, " = ");
				option_types[opt->type].write(opt, mirror);
				state->mirrored = state->pos.look;
			}
			/* Remember that the option need not be
			 * written to the end of the file.  */
			flagsite->flags &= ~OPT_MUST_SAVE;
		}
	}

	return ERROR_NONE;
}

static enum parse_error
parse_bind(struct option *opt_tree, struct conf_parsing_state *state,
	   struct string *mirror, int is_system_conf)
{
	unsigned char *keymap, *keystroke, *action;
	enum parse_error err = ERROR_NONE;
	struct conf_parsing_pos before_error;

	skip_white(&state->pos);
	if (!*state->pos.look) return show_parse_error(state, ERROR_PARSE);

	/* Keymap */
	before_error = state->pos;
	keymap = option_types[OPT_STRING].read(NULL, &state->pos.look,
					       &state->pos.line);
	skip_white(&state->pos);
	if (!keymap || !*state->pos.look) {
		state->pos = before_error;
		return show_parse_error(state, ERROR_PARSE);
	}

	/* Keystroke */
	before_error = state->pos;
	keystroke = option_types[OPT_STRING].read(NULL, &state->pos.look,
						  &state->pos.line);
	skip_white(&state->pos);
	if (!keystroke || !*state->pos.look) {
		mem_free(keymap); mem_free_if(keystroke);
		state->pos = before_error;
		return show_parse_error(state, ERROR_PARSE);
	}

	/* Equal sign */
	skip_white(&state->pos);
	if (*state->pos.look != '=') {
		mem_free(keymap); mem_free(keystroke);
		return show_parse_error(state, ERROR_PARSE);
	}
	state->pos.look++; /* '=' */

	skip_white(&state->pos);
	if (!*state->pos.look) {
		mem_free(keymap); mem_free(keystroke);
		return show_parse_error(state, ERROR_PARSE);
	}

	/* Action */
	before_error = state->pos;
	action = option_types[OPT_STRING].read(NULL, &state->pos.look,
					       &state->pos.line);
	if (!action) {
		mem_free(keymap); mem_free(keystroke);
		state->pos = before_error;
		return show_parse_error(state, ERROR_PARSE);
	}

	if (!mirror) {
		/* loading a configuration file */
		/* We don't bother to bind() if -default-keys. */
		if (!get_cmd_opt_bool("default-keys")
		    && bind_do(keymap, keystroke, action, is_system_conf)) {
			/* bind_do() tried but failed. */
			err = show_parse_error(state, ERROR_VALUE);
		} else {
			err = ERROR_NONE;
		}
	} else if (is_system_conf) {
		/* scanning a file that will not be rewritten */
		/* TODO */
	} else {
		/* rewriting a configuration file */
		/* Mirror what we already have.  If the keystroke has
		 * been unbound, then act_str is simply "none" and
		 * this does not require special handling.  */
		unsigned char *act_str = bind_act(keymap, keystroke);

		if (act_str) {
			add_bytes_to_string(mirror, state->mirrored,
					    before_error.look - state->mirrored);
			add_to_string(mirror, act_str);
			mem_free(act_str);
			state->mirrored = state->pos.look;
		} else {
			err = show_parse_error(state, ERROR_VALUE);
		}
	}
	mem_free(keymap); mem_free(keystroke); mem_free(action);
	return err;
}

static int load_config_file(unsigned char *, unsigned char *, struct option *,
			    struct string *, int);

static enum parse_error
parse_include(struct option *opt_tree, struct conf_parsing_state *state,
	      struct string *mirror, int is_system_conf)
{
	unsigned char *fname;
	struct string dumbstring;
	struct conf_parsing_pos before_error;

	if (!init_string(&dumbstring))
		return show_parse_error(state, ERROR_NOMEM);

	skip_white(&state->pos);
	if (!*state->pos.look) {
		done_string(&dumbstring);
		return show_parse_error(state, ERROR_PARSE);
	}

	/* File name */
	before_error = state->pos;
	fname = option_types[OPT_STRING].read(NULL, &state->pos.look,
					      &state->pos.line);
	if (!fname) {
		done_string(&dumbstring);
		state->pos = before_error;
		return show_parse_error(state, ERROR_PARSE);
	}

	/* We want load_config_file() to watermark stuff, but not to load
	 * anything, polluting our beloved options tree - thus, we will feed it
	 * with some dummy string which we will destroy later; still better
	 * than cloning whole options tree or polluting interface with another
	 * rarely-used option ;). */
	/* XXX: We should try CONFDIR/<file> when proceeding
	 * CONFDIR/<otherfile> ;). --pasky */
	if (load_config_file(fname[0] == '/' ? (unsigned char *) ""
					     : elinks_home,
			     fname, opt_tree, 
			     mirror ? &dumbstring : NULL, 1)) {
		done_string(&dumbstring);
		mem_free(fname);
		return show_parse_error(state, ERROR_VALUE);
	}

	done_string(&dumbstring);
	mem_free(fname);
	return ERROR_NONE;
}


struct parse_handler {
	const unsigned char *command;
	enum parse_error (*handler)(struct option *opt_tree,
				    struct conf_parsing_state *state,
				    struct string *mirror, int is_system_conf);
};

static const struct parse_handler parse_handlers[] = {
	{ "set", parse_set },
	{ "unset", parse_unset },
	{ "bind", parse_bind },
	{ "include", parse_include },
	{ NULL, NULL }
};


static enum parse_error
parse_config_command(struct option *options, struct conf_parsing_state *state,
		     struct string *mirror, int is_system_conf)
{
	const struct parse_handler *handler;

	/* If we're mirroring, then everything up to this point must
	 * have already been mirrored.  */
	assert(mirror == NULL || state->mirrored == state->pos.look);
	if_assert_failed return show_parse_error(state, ERROR_PARSE);

	for (handler = parse_handlers; handler->command;
	     handler++) {
		int cmdlen = strlen(handler->command);

		if (!strncmp(state->pos.look, handler->command, cmdlen)
		    && isspace(state->pos.look[cmdlen])) {
			enum parse_error err;

			state->pos.look += cmdlen;
			err = handler->handler(options, state, mirror,
			                       is_system_conf);
			if (mirror) {
				/* Mirror any characters that the handler
				 * consumed but did not already mirror.  */
				add_bytes_to_string(mirror, state->mirrored,
						    state->pos.look - state->mirrored);
				state->mirrored = state->pos.look;
			}
			return err;
		}
	}

	return show_parse_error(state, ERROR_COMMAND);
}

#ifdef CONFIG_EXMODE
enum parse_error
parse_config_exmode_command(unsigned char *cmd)
{
	struct conf_parsing_state state = {{ 0 }};

	state.pos.look = cmd;
	state.pos.line = 0;
	state.mirrored = NULL; /* not read because mirror is NULL too */
	state.filename = NULL; /* prevent error messages */

	return parse_config_command(config_options, &state, NULL, 0);
}
#endif /* CONFIG_EXMODE */

void
parse_config_file(struct option *options, unsigned char *name,
		  unsigned char *file, struct string *mirror,
		  int is_system_conf)
{
	struct conf_parsing_state state = {{ 0 }};
	int error_occurred = 0;

	state.pos.look = file;
	state.pos.line = 1;
	state.mirrored = file;
	if (!mirror && get_cmd_opt_int("verbose") >= VERBOSE_WARNINGS)
		state.filename = name;

	while (state.pos.look && *state.pos.look) {
		enum parse_error err = ERROR_NONE;

		/* Skip all possible comments and whitespace. */
		skip_white(&state.pos);

		/* Mirror what we already have */
		if (mirror) {
			add_bytes_to_string(mirror, state.mirrored,
					    state.pos.look - state.mirrored);
			state.mirrored = state.pos.look;
		}

		/* Second chance to escape from the hell. */
		if (!*state.pos.look) break;

		err = parse_config_command(options, &state, mirror,
		                           is_system_conf);
		switch (err) {
		case ERROR_NONE:
			break;

		case ERROR_COMMAND:
		case ERROR_PARSE:
			/* Jump over this crap we can't understand. */
			skip_to_unquoted_newline_or_comment(&state.pos);

			/* Mirror what we already have */
			if (mirror) {
				add_bytes_to_string(mirror, state.mirrored,
						    state.pos.look - state.mirrored);
				state.mirrored = state.pos.look;
			}

			/* fall through */
		default:
			error_occurred = 1;
			break;
		}
	}

	if (!error_occurred || !state.filename) return;

	/* If an error occurred make sure that the user is notified and is able
	 * to see it. First sound the bell. Then, if the text viewer is going to
	 * be started, sleep for a while so the message will be visible before
	 * ELinks starts drawing to on the screen and overwriting any error
	 * messages. This should not be necessary for terminals supporting the
	 * alternate screen mode but better to be safe. (debian bug 305017) */

	fputc('\a', stderr);

	if (get_cmd_opt_bool("dump")
	    || get_cmd_opt_bool("source"))
		return;

	sleep(1);
}



static unsigned char *
read_config_file(unsigned char *name)
{
#define FILE_BUF	1024
	unsigned char cfg_buffer[FILE_BUF];
	struct string string;
	int fd;
	ssize_t r;

	fd = open(name, O_RDONLY | O_NOCTTY);
	if (fd < 0) return NULL;
	set_bin(fd);

	if (!init_string(&string)) return NULL;

	while ((r = safe_read(fd, cfg_buffer, FILE_BUF)) > 0) {
		int i;

		/* Clear problems ;). */
		for (i = 0; i < r; i++)
			if (!cfg_buffer[i])
				cfg_buffer[i] = ' ';

		add_bytes_to_string(&string, cfg_buffer, r);
	}

	if (r < 0) done_string(&string);
	close(fd);

	return string.source;
#undef FILE_BUF
}

/* Return 0 on success. */
static int
load_config_file(unsigned char *prefix, unsigned char *name,
		 struct option *options, struct string *mirror,
		 int is_system_conf)
{
	unsigned char *config_str, *config_file;

	config_file = straconcat(prefix, STRING_DIR_SEP, name,
				 (unsigned char *) NULL);
	if (!config_file) return 1;

	config_str = read_config_file(config_file);
	if (!config_str) {
		mem_free(config_file);
		config_file = straconcat(prefix, STRING_DIR_SEP, ".", name,
					 (unsigned char *) NULL);
		if (!config_file) return 2;

		config_str = read_config_file(config_file);
		if (!config_str) {
			mem_free(config_file);
			return 3;
		}
	}

	parse_config_file(options, config_file, config_str, mirror,
	                  is_system_conf);

	mem_free(config_str);
	mem_free(config_file);

	return 0;
}

static void
load_config_from(unsigned char *file, struct option *tree)
{
	load_config_file(CONFDIR, file, tree, NULL, 1);
	load_config_file(empty_string_or_(elinks_home), file, tree, NULL, 0);
}

void
load_config(void)
{
	load_config_from(get_cmd_opt_str("config-file"),
			 config_options);
}


static int indentation = 2;
/* 0 -> none, 1 -> only option full name+type, 2 -> only desc, 3 -> both */
static int comments = 3;

static inline unsigned char *
conf_i18n(unsigned char *s, int i18n)
{
	if (i18n) return gettext(s);
	return s;
}

static void
add_indent_to_string(struct string *string, int depth)
{
	if (!depth) return;

	add_xchar_to_string(string, ' ', depth * indentation);
}

struct string *
wrap_option_desc(struct string *out, const unsigned char *src,
		 const struct string *indent, int maxwidth)
{
	const unsigned char *last_space = NULL;
	const unsigned char *uncopied = src;
	int width = 0;

	/* TODO: multibyte or fullwidth characters */
	for (; *src; src++, width++) {
		if (*src == '\n') {
			last_space = src;
			goto split;
		}

		if (*src == ' ') last_space = src;

		if (width >= maxwidth && last_space) {
split:
			if (!add_string_to_string(out, indent))
				return NULL;
			if (!add_bytes_to_string(out, uncopied,
						 last_space - uncopied))
				return NULL;
			if (!add_char_to_string(out, '\n'))
				return NULL;
			uncopied = last_space + 1;
			width = src - uncopied;
			last_space = NULL;
		}
	}
	if (*uncopied) {
		if (!add_string_to_string(out, indent))
			return NULL;
		if (!add_to_string(out, uncopied))
			return NULL;
		if (!add_char_to_string(out, '\n'))
			return NULL;
	}
	return out;
}

static void
output_option_desc_as_comment(struct string *out, const struct option *option,
			      int i18n, int depth)
{
	unsigned char *desc_i18n = conf_i18n(option->desc, i18n);
	struct string indent;

	if (!init_string(&indent)) return;

	add_indent_to_string(&indent, depth);
	if (!add_to_string(&indent, "#  ")) goto out_of_memory;
	if (!wrap_option_desc(out, desc_i18n, &indent, 80 - indent.length))
		goto out_of_memory;

out_of_memory:
	done_string(&indent);
}

static void
smart_config_output_fn(struct string *string, struct option *option,
		       unsigned char *path, int depth, int do_print_comment,
		       int action, int i18n)
{
	if (option->type == OPT_ALIAS)
		return;

	switch (action) {
		case 0:
			if (!(comments & 1)) break;

			add_indent_to_string(string, depth);

			add_to_string(string, "## ");
			if (path) {
				add_to_string(string, path);
				add_char_to_string(string, '.');
			}
			add_to_string(string, option->name);
			add_char_to_string(string, ' ');
			add_to_string(string, option_types[option->type].help_str);
			add_char_to_string(string, '\n');
			break;

		case 1:
			if (!(comments & 2)) break;

			if (!option->desc || !do_print_comment)
				break;

			output_option_desc_as_comment(string, option,
						      i18n, depth);
			break;

		case 2:

			add_indent_to_string(string, depth);
			if (option->flags & OPT_DELETED) {
				add_to_string(string, "un");
			}
			add_to_string(string, "set ");
			if (path) {
				add_to_string(string, path);
				add_char_to_string(string, '.');
			}
			add_to_string(string, option->name);
			if (!(option->flags & OPT_DELETED)) {
				add_to_string(string, " = ");
				/* OPT_ALIAS won't ever. OPT_TREE won't reach action 2.
				 * OPT_SPECIAL makes no sense in the configuration
				 * context. */
				assert(option_types[option->type].write);
				option_types[option->type].write(option, string);
			}
			add_char_to_string(string, '\n');
			if (do_print_comment) add_char_to_string(string, '\n');
			break;

		case 3:
			if (do_print_comment < 2)
				add_char_to_string(string, '\n');
			break;
	}
}


static void
add_cfg_header_to_string(struct string *string, unsigned char *text)
{
	int n = strlen(text) + 2;

	int_bounds(&n, 10, 80);

	add_to_string(string, "\n\n\n");
	add_xchar_to_string(string, '#', n);
	add_to_string(string, "\n# ");
	add_to_string(string, text);
	add_to_string(string, "#\n\n");
}

unsigned char *
create_config_string(unsigned char *prefix, unsigned char *name,
		     struct option *options)
{
	struct string config;
	/* Don't write headers if nothing will be added anyway. */
	struct string tmpstring;
	int origlen;
	int savestyle = get_opt_int("config.saving_style");
	int i18n = get_opt_bool("config.i18n");

	if (!init_string(&config)) return NULL;

	prepare_mustsave_flags(options->value.tree,
			       savestyle == 1 || savestyle == 2);

	/* Scaring. */
	if (savestyle == 2
	    || load_config_file(prefix, name, options, &config, 0)
	    || !config.length) {
		/* At first line, and in English, write ELinks version, may be
		 * of some help in future. Please keep that format for it.
		 * --Zas */
		add_to_string(&config, "## ELinks " VERSION " configuration file\n\n");
		assert(savestyle >= 0  && savestyle <= 3);
		switch (savestyle) {
		case 0:
			add_to_string(&config, conf_i18n(N_(
			"## This is ELinks configuration file. You can edit it manually,\n"
			"## if you wish so; this file is edited by ELinks when you save\n"
			"## options through UI, however only option values will be altered\n"
			"## and all your formatting, own comments etc will be kept as-is.\n"),
			i18n));
			break;
		case 1: case 3:
			add_to_string(&config, conf_i18n(N_(
			"## This is ELinks configuration file. You can edit it manually,\n"
			"## if you wish so; this file is edited by ELinks when you save\n"
			"## options through UI, however only option values will be altered\n"
			"## and missing options will be added at the end of file; if option\n"
			"## is not written in this file, but in some file included from it,\n"
			"## it is NOT counted as missing. Note that all your formatting,\n"
			"## own comments and so on will be kept as-is.\n"), i18n));
			break;
		case 2:
			add_to_string(&config, conf_i18n(N_(
			"## This is ELinks configuration file. You can edit it manually,\n"
			"## if you wish so, but keep in mind that this file is overwritten\n"
			"## by ELinks when you save options through UI and you are out of\n"
			"## luck with your formatting and own comments then, so beware.\n"),
			i18n));
			break;
		}

		add_to_string(&config, "##\n");

		add_to_string(&config, conf_i18n(N_(
			"## Obviously, if you don't like what ELinks is going to do with\n"
			"## this file, you can change it by altering the config.saving_style\n"
			"## option. Come on, aren't we friendly guys after all?\n"), i18n));
	}

	if (savestyle == 0) goto get_me_out;

	indentation = get_opt_int("config.indentation");
	comments = get_opt_int("config.comments");

	if (!init_string(&tmpstring)) goto get_me_out;

	add_cfg_header_to_string(&tmpstring,
				 conf_i18n(N_("Automatically saved options\n"), i18n));

	origlen = tmpstring.length;
	smart_config_string(&tmpstring, 2, i18n, options->value.tree, NULL, 0,
			    smart_config_output_fn);
	if (tmpstring.length > origlen)
		add_string_to_string(&config, &tmpstring);
	done_string(&tmpstring);

	if (!init_string(&tmpstring)) goto get_me_out;

	add_cfg_header_to_string(&tmpstring,
				 conf_i18n(N_("Automatically saved keybindings\n"), i18n));

	origlen = tmpstring.length;
	bind_config_string(&tmpstring);
	if (tmpstring.length > origlen)
		add_string_to_string(&config, &tmpstring);
	done_string(&tmpstring);

get_me_out:
	return config.source;
}

static int
write_config_file(unsigned char *prefix, unsigned char *name,
		  struct option *options, struct terminal *term)
{
	int ret = -1;
	struct secure_save_info *ssi;
	unsigned char *config_file = NULL;
	unsigned char *cfg_str = create_config_string(prefix, name, options);
	int prefixlen = strlen(prefix);
	int prefix_has_slash = (prefixlen && dir_sep(prefix[prefixlen - 1]));
	int name_has_slash = dir_sep(name[0]);
	unsigned char *slash = name_has_slash || prefix_has_slash ? "" : STRING_DIR_SEP;

	if (!cfg_str) return -1;

	if (name_has_slash && prefix_has_slash) name++;

	config_file = straconcat(prefix, slash, name, (unsigned char *) NULL);
	if (!config_file) goto free_cfg_str;

	ssi = secure_open(config_file);
	if (ssi) {
		secure_fputs(ssi, cfg_str);
		ret = secure_close(ssi);
		if (!ret)
			untouch_options(options->value.tree);
	}

	write_config_dialog(term, config_file, secsave_errno, ret);
	mem_free(config_file);

free_cfg_str:
	mem_free(cfg_str);

	return ret;
}

int
write_config(struct terminal *term)
{
	assert(term);

	if (!elinks_home) {
		write_config_dialog(term, get_cmd_opt_str("config-file"),
				    SS_ERR_DISABLED, 0);
		return -1;
	}

	return write_config_file(elinks_home, get_cmd_opt_str("config-file"),
				 config_options, term);
}
