/* Option variables types handlers */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* Commandline handlers. */

/* TAKE CARE! Remember that your _rd handler can be used for commandline
 * parameters as well - probably, you don't want to be so syntactically
 * strict, and _ESPECIALLY_ you don't want to move any file pointers ahead,
 * since you will parse the commandline _TWO TIMES_! Remember! :-) */
int commandline = 0;

static unsigned char *
gen_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	unsigned char *str;
	int dummy_line = 0;

	if (!*argc) return gettext("Parameter expected");

	/* FIXME!! We will modify argv! (maybe) */
	commandline = 1;
	str = option_types[o->type].read(o, *argv, &dummy_line);
	commandline = 0;
	if (str) {
		/* We ate parameter */
		(*argv)++; (*argc)--;
		if (option_types[o->type].set(o, str)) {
			mem_free(str);
			return NULL;
		}
		mem_free(str);
	}

	return gettext("Read error");
}

/* If 0 follows, disable option and eat 0. If 1 follows, enable option and
 * eat 1. If anything else follow, enable option and don't eat anything. */
static unsigned char *
bool_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	o->value.number = 1;

	if (!*argc) return NULL;

	/* Argument is empty or longer than 1 char.. */
	if (!(*argv)[0][0] || (*argv)[0][1]) return NULL;

	switch ((*argv)[0][0]) {
		case '0': o->value.number = 0; break;
		case '1': o->value.number = 1; break;
		default: return NULL;
	}

	/* We ate parameter */
	(*argv)++; (*argc)--;
	return NULL;
}

static unsigned char *
exec_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	return o->value.command(o, argv, argc);
}


/* Wrappers for OPT_ALIAS. */
/* Note that they can wrap only to config_options now.  I don't think it could be
 * a problem, but who knows.. however, changing that will be pretty tricky -
 * possibly changing ptr to structure containing target name and pointer to
 * options list? --pasky */

static unsigned char *
redir_cmd(struct option *opt, unsigned char ***argv, int *argc)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);
	unsigned char * ret = NULL;

	assertm(real != NULL, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return ret; }

	if (option_types[real->type].cmdline) {
		ret = option_types[real->type].cmdline(real, argv, argc);
		if ((opt->flags & OPT_ALIAS_NEGATE) && real->type == OPT_BOOL) {
			real->value.number = !real->value.number;
		}
	}

	return ret;
}

static unsigned char *
redir_rd(struct option *opt, unsigned char **file, int *line)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);
	unsigned char *ret = NULL;

	assertm(real != NULL, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return ret; }

	if (option_types[real->type].read) {
		ret = option_types[real->type].read(real, file, line);
		if (ret && (opt->flags & OPT_ALIAS_NEGATE) && real->type == OPT_BOOL) {
			*(long *) ret = !*(long *) ret;
		}
	}

	return ret;
}

static void
redir_wr(struct option *opt, struct string *string)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);

	assertm(real != NULL, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return; }

	if (option_types[real->type].write)
		option_types[real->type].write(real, string);
}

static int
redir_set(struct option *opt, unsigned char *str)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);
	int ret = 0;

	assertm(real != NULL, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return ret; }

	if (option_types[real->type].set) {
		long negated;

		if ((opt->flags & OPT_ALIAS_NEGATE) && real->type == OPT_BOOL) {
			negated = !*(long *) str;
			str = (unsigned char *) &negated;
		}
		ret = option_types[real->type].set(real, str);
	}

	return ret;
}

static int
redir_eq(struct option *opt, const unsigned char *str)
{
	struct option *real = get_opt_rec(config_options, opt->value.string);
	int ret = 0;

	assertm(real != NULL, "%s aliased to unknown option %s!", opt->name, opt->value.string);
	if_assert_failed { return ret; }

	if (option_types[real->type].equals) {
		long negated;

		if ((opt->flags & OPT_ALIAS_NEGATE) && real->type == OPT_BOOL) {
			negated = !*(const long *) str;
			str = (unsigned char *) &negated;
		}
		ret = option_types[real->type].equals(real, str);
	}

	return ret;
}



/* Support functions for config file parsing. */

static void
add_optstring_to_string(struct string *s, const unsigned char *q, int qlen)
{
 	if (!commandline) add_char_to_string(s, '"');
	add_quoted_to_string(s, q, qlen);
	if (!commandline) add_char_to_string(s, '"');
}

/* Config file handlers. */

static unsigned char *
num_rd(struct option *opt, unsigned char **file, int *line)
{
	unsigned char *end = *file;
	long *value = mem_alloc(sizeof(*value));

	if (!value) return NULL;

	/* We don't want to move file if (commandline), but strtolx() second
	 * parameter must not be NULL. */
	*value = strtolx(*file, &end);
	if (!commandline) *file = end;

	/* Another trap for unwary - we need to check *end, not **file - reason
	 * is left as an exercise to the reader. */
	if ((*end != 0 && (commandline || (!isspace(*end) && *end != '#')))
	    || (*value < opt->min || *value > opt->max)) {
		mem_free(value);
		return NULL;
	}

	return (unsigned char *) value;
}

static int
num_set(struct option *opt, unsigned char *str)
{
	opt->value.number = *((long *) str);
	return 1;
}

static int
num_eq(struct option *opt, const unsigned char *str)
{
	return str && opt->value.number == *(const long *) str;
}

static void
num_wr(struct option *option, struct string *string)
{
	add_knum_to_string(string, option->value.number);
}


static int
long_set(struct option *opt, unsigned char *str)
{
	opt->value.big_number = *((long *) str);
	return 1;
}

static int
long_eq(struct option *opt, const unsigned char *str)
{
	return str && opt->value.big_number == *(const long *) str;
}

static void
long_wr(struct option *option, struct string *string)
{
	add_knum_to_string(string, option->value.big_number);
}

static unsigned char *
str_rd(struct option *opt, unsigned char **file, int *line)
{
	unsigned char *str = *file;
	struct string str2;

	if (!init_string(&str2)) return NULL;

	/* We're getting used in some parser functions in conf.c as well, and
	 * that's w/ opt == NULL; so don't rely on opt to point anywhere. */
	if (!commandline) {
		if (!isquote(*str)) {
			done_string(&str2);
			return NULL;
		}
		str++;
	}

	while (*str && (commandline || !isquote(*str))) {
		if (*str == '\\') {
			/* FIXME: This won't work on crlf systems. */
			if (str[1] == '\n') { str[1] = ' '; str++; (*line)++; }
			/* When there's quote char, we will just move on there,
			 * thus we will never test for it in while () condition
			 * and we will treat it just as '"', ignoring the
			 * backslash itself. */
			else if (isquote(str[1])) str++;
			/* \\ means \. */
			else if (str[1] == '\\') str++;
		}

		if (*str == '\n') (*line)++;

		add_char_to_string(&str2, *str);
		str++;
	}

	if (!commandline && !*str) {
		done_string(&str2);
		*file = str;
		return NULL;
	}

	str++; /* Skip the quote. */
	if (!commandline) *file = str;

	if (opt && opt->max && str2.length >= opt->max) {
		done_string(&str2);
		return NULL;
	}

	return str2.source;
}

static int
str_set(struct option *opt, unsigned char *str)
{
	assert(opt->value.string);

	safe_strncpy(opt->value.string, str, MAX_STR_LEN);
	return 1;
}

static int
str_eq(struct option *opt, const unsigned char *str)
{
	return str && strcmp(opt->value.string, str) == 0;
}

static void
str_wr(struct option *o, struct string *s)
{
	int len = strlen(o->value.string);

	int_upper_bound(&len, o->max - 1);
	add_optstring_to_string(s, o->value.string, len);
}

static void
str_dup(struct option *opt, struct option *template)
{
	unsigned char *new = mem_alloc(MAX_STR_LEN);

	if (new) safe_strncpy(new, template->value.string, MAX_STR_LEN);
	opt->value.string = new;
}


static int
cp_set(struct option *opt, unsigned char *str)
{
	int ret = get_cp_index(str);

	if (ret < 0) return 0;

	opt->value.number = ret;
	return 1;
}

static int
cp_eq(struct option *opt, const unsigned char *str)
{
	return str && get_cp_index(str) == opt->value.number;
}

static void
cp_wr(struct option *o, struct string *s)
{
	unsigned char *mime_name = get_cp_config_name(o->value.number);

	add_optstring_to_string(s, mime_name, strlen(mime_name));
}


static int
lang_set(struct option *opt, unsigned char *str)
{
#ifdef CONFIG_NLS
	opt->value.number = name_to_language(str);
	set_language(opt->value.number);
#endif
	return 1;
}

static int
lang_eq(struct option *opt, const unsigned char *str)
{
#ifdef CONFIG_NLS
	return str && name_to_language(str) == opt->value.number;
#else
	return 1;		/* All languages are the same.  */
#endif
}

static void
lang_wr(struct option *o, struct string *s)
{
	unsigned char *lang;

#ifdef CONFIG_NLS
	lang = language_to_name(current_language);
#else
	lang = "System";
#endif

	add_optstring_to_string(s, lang, strlen(lang));
}


static int
color_set(struct option *opt, unsigned char *str)
{
	return !decode_color(str, strlen(str), &opt->value.color);
}

static int
color_eq(struct option *opt, const unsigned char *str)
{
	color_T color;

	return str && !decode_color(str, strlen(str), &color)
		&& color == opt->value.color;
}

static void
color_wr(struct option *opt, struct string *str)
{
	color_T color = opt->value.color;
	unsigned char hexcolor[8];
	const unsigned char *strcolor = get_color_string(color, hexcolor);

	add_optstring_to_string(str, strcolor, strlen(strcolor));
}

static void
tree_dup(struct option *opt, struct option *template)
{
	LIST_OF(struct option) *new = init_options_tree();
	LIST_OF(struct option) *tree = template->value.tree;
	struct option *option;

	if (!new) return;
	opt->value.tree = new;

	foreachback (option, *tree) {
		struct option *new_opt = copy_option(option);

		if (!new_opt) continue;
		object_nolock(new_opt, "option");
		add_to_list_end(*new, new_opt);
		new_opt->root = opt;

		if (!new_opt->box_item) continue;

		if (new_opt->name && !strcmp(new_opt->name, "_template_"))
			new_opt->box_item->visible = get_opt_bool("config.show_template");

		if (opt->box_item) {
			add_to_list(opt->box_item->child,
				    new_opt->box_item);
		}
	}
}

const struct option_type_info option_types[] = {
	/* The OPT_ comments below are here to be found by grep.  */

	/* OPT_BOOL */
	{ N_("Boolean"),  bool_cmd,  num_rd,   num_wr,   NULL,     num_set,   num_eq,   N_("[0|1]") },
	/* OPT_INT */
	{ N_("Integer"),  gen_cmd,   num_rd,   num_wr,   NULL,     num_set,   num_eq,   N_("<num>") },
	/* OPT_LONG */
	{ N_("Longint"),  gen_cmd,   num_rd,   long_wr,  NULL,     long_set,  long_eq,  N_("<num>") },
	/* OPT_STRING */
	{ N_("String"),   gen_cmd,   str_rd,   str_wr,   str_dup,  str_set,   str_eq,   N_("<str>") },

	/* OPT_CODEPAGE */
	{ N_("Codepage"), gen_cmd,   str_rd,   cp_wr,    NULL,     cp_set,    cp_eq,    N_("<codepage>") },
	/* OPT_LANGUAGE */
	{ N_("Language"), gen_cmd,   str_rd,   lang_wr,  NULL,     lang_set,  lang_eq,  N_("<language>") },
	/* OPT_COLOR */
	{ N_("Color"),    gen_cmd,   str_rd,   color_wr, NULL,     color_set, color_eq, N_("<color|#rrggbb>") },

	/* OPT_COMMAND */
	{ N_("Special"),  exec_cmd,  NULL,     NULL,     NULL,     NULL,      NULL,     "" },

	/* OPT_ALIAS */
	{ N_("Alias"),    redir_cmd, redir_rd, redir_wr, NULL,     redir_set, redir_eq, "" },

	/* OPT_TREE */
	{ N_("Folder"),   NULL,      NULL,     NULL,     tree_dup, NULL,      NULL,      "" },
};

unsigned char *
get_option_type_name(enum option_type type)
{
	assert(type >= 0 && type < sizeof(option_types)/sizeof(struct option_type_info));
	if_assert_failed return "";

	return option_types[type].name;
}
