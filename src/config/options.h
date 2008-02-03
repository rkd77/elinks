
#ifndef EL__CONFIG_OPTIONS_H
#define EL__CONFIG_OPTIONS_H

#include "main/object.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"

/* TODO: We should provide some generic mechanism for options caching. */

/* Fix namespace clash on Cygwin. */
#define option option_elinks


enum option_flags {
	/* bitmask */
	/* The option is hidden - it serves for internal purposes, never is
	 * read, never is written, never is displayed, never is crawled through
	 * etc. */
	OPT_HIDDEN = 1,
	/* For OPT_TREE, automatically create missing hiearchy piece just under
	 * this category when adding an option. The 'template' for the added
	 * hiearchy piece (category) is stored as "_template_" category. */
	OPT_AUTOCREATE = 2,
	/* The option has been modified in some way and must be saved
	 * to elinks.conf.  ELinks uses this flag only while it is
	 * saving the options.  When the config.saving_style option
	 * has value 3, saving works like this:
	 * - First, ELinks sets OPT_MUST_SAVE in the options that have
	 *   OPT_TOUCHED or OPT_DELETED, and clears it in the rest.
	 * - ELinks then parses the old configuration file and any
	 *   files named in "include" commands.
	 * - If the old configuration file contains a "set" or "unset"
	 *   command for this option, ELinks rewrites the command and
	 *   clears OPT_MUST_SAVE.
	 * - If an included file contains a "set" or "unset" command
	 *   for this option, ELinks compares the value of the option
	 *   to the value given in the command.  ELinks clears
	 *   OPT_MUST_SAVE if the values match, or sets it if they
	 *   differ.
	 * - After ELinks has rewritten the configuration file and
	 *   parsed the included files, it appends the options that
	 *   still have the OPT_MUST_SAVE flag.
	 * Other saving styles are variants of this:
	 * - 0: ELinks does not append any options to the
	 *   configuration file.  So OPT_MUST_SAVE has no effect.
	 * - 1: ELinks initially sets OPT_MUST_SAVE in all options,
	 *   regardless of OPT_TOUCHED and OPT_DELETED.
	 * - 2: ELinks initially sets OPT_MUST_SAVE in all options,
	 *   and does not read any configuration files.  */
	OPT_MUST_SAVE = 4,
	/* This is used to mark options modified after the last save. That's
	 * being useful if you want to save only the options whose value
	 * changed. */
	OPT_TOUCHED = 8,
	/* If set on the tree argument to add_opt (not necessarily the direct
	 * parent) or on the option itself, it will create the listbox (options
	 * manager) item for the option. */
	OPT_LISTBOX = 16,
	/* This is used to mark that the option _and_ the option name is
	 * allocated and should be freed when the option is released. */
	OPT_ALLOC = 32,
	/* For OPT_TREE, automatically sort the content of the tree
	 * alphabetically (but all subtrees in front of ordinary options) when
	 * adding new options. Note that this applies only to the one level
	 * below - it will not apply to the sub-trees in this tree. Also, this
	 * can be quite expensive for busy-adding big trees, so think twice
	 * before doing it - in fact, it is supposed to be used only where you
	 * add stuff from more modules, not all at once; typically the
	 * config_options root tree. Especially NOT RECOMMENDED to be used on
	 * the template trees. */
	OPT_SORT = 64,
	/* This is used to mark option as deleted */
	OPT_DELETED = 128,
	/* Specifies that values of boolean aliases should be inverted. */
	OPT_ALIAS_NEGATE = 256
};

enum option_type {
	OPT_BOOL = 0,
	OPT_INT,
	OPT_LONG,
	OPT_STRING,

	OPT_CODEPAGE,
	OPT_LANGUAGE,
	OPT_COLOR,

	OPT_COMMAND,

	OPT_ALIAS,

	OPT_TREE,
};

struct listbox_item; /* bfu/listbox.h */
struct option; /* defined later in this file */
struct session; /* session/session.h */

union option_value {
	/* XXX: Keep first to make @options_root initialization possible. */
	/* The OPT_TREE list_head is allocated. */
	LIST_OF(struct option) *tree;

	/* Used by OPT_BOOL, OPT_INT, OPT_CODEPAGE and OPT_LANGUAGE */
	int number;

	/* Used by OPT_LONG */
	long big_number;

	/* The OPT_COLOR value */
	color_T color;

	/* The OPT_COMMAND value */
	unsigned char *(*command)(struct option *, unsigned char ***, int *);

	/* The OPT_STRING string is allocated and has length MAX_STR_LEN.
	 * The OPT_ALIAS string is NOT allocated, has variable length
	 * (opt->max) and should remain untouched! It contains the full path to
	 * the "real" / aliased option. */
	unsigned char *string;
};


/* @session is the session via which the user changed the options,
 * or NULL if not known.  Because the options are currently not
 * session-specific, it is best to ignore this parameter.  In a future
 * version of ELinks, this parameter might mean the session to which
 * the changed options apply.
 *
 * @current is the option whose change hook is being called.  It is
 * never NULL.
 *
 * @changed is the option that was changed, or NULL if multiple
 * descendants of @current may have been changed.  */
typedef int (*change_hook_T)(struct session *session, struct option *current,
			     struct option *changed);

struct option {
	OBJECT_HEAD(struct option);

	unsigned char *name;
	enum option_flags flags;
	enum option_type type;
	long min, max;
	union option_value value;
	unsigned char *desc;
	unsigned char *capt;

	struct option *root;

	/* To be called when the option (or sub-option if it's a tree) is
	 * changed. If it returns zero, we will continue descending the options
	 * tree checking for change handlers. */
	change_hook_T change_hook;

	struct listbox_item *box_item;
};

#define INIT_OPTION(name, flags, type, min, max, value, desc, capt) \
	{ NULL_LIST_HEAD, INIT_OBJECT("option"), name, flags, type, min, max, { (LIST_OF(struct option) *) (value) }, desc, capt }

extern struct option *config_options;
extern struct option *cmdline_options;


extern void init_options(void);
extern void done_options(void);


struct change_hook_info {
	unsigned char *name;
	change_hook_T change_hook;
};

extern void register_change_hooks(const struct change_hook_info *change_hooks);


extern LIST_OF(struct option) *init_options_tree(void);
extern void prepare_mustsave_flags(LIST_OF(struct option) *, int set_all);
extern void untouch_options(LIST_OF(struct option) *);

extern void smart_config_string(struct string *, int, int,
				LIST_OF(struct option) *, unsigned char *, int,
				void (*)(struct string *, struct option *,
					 unsigned char *, int, int, int, int));

extern struct option *copy_option(struct option *);
extern void delete_option(struct option *);
void mark_option_as_deleted(struct option *);

/* Some minimal option cache */

struct option_resolver {
	int id;
	unsigned char *name;
};

/* Update the visibility of the box item of each option
 * in config_options to honour the value of config.show_template. */
void update_options_visibility(void);

/* Toggle the value of the given option numeric, respecting option->min
 * and option->max. */
void toggle_option(struct session *ses, struct option *option);

/* Call the change-hooks for the given option and recur on its parent. */
void call_change_hooks(struct session *ses, struct option *current,
                       struct option *option);

/* Do proper bookkeeping after an option has changed - call this every time
 * you change an option value. */
void option_changed(struct session *ses, struct option *option);

extern int commit_option_values(struct option_resolver *resolvers,
				struct option *root,
				union option_value *values, int size);
extern void checkout_option_values(struct option_resolver *resolvers,
				   struct option *root,
				   union option_value *values, int size);

/* Shitload of various incredible macro combinations and other unusable garbage
 * follows. Have fun. */

/* Basically, for main hiearchy addressed from root (almost always) you want to
 * use get_opt_type() and add_opt_type(). For command line options, you want to
 * use get_opt_type_tree(cmdline_options, "option"). */

extern struct option *get_opt_rec(struct option *, const unsigned char *);
extern struct option *get_opt_rec_real(struct option *, const unsigned char *);
struct option *indirect_option(struct option *);
#ifdef CONFIG_DEBUG
extern union option_value *get_opt_(unsigned char *, int, enum option_type, struct option *, unsigned char *);
#define get_opt(tree, name, type) get_opt_(__FILE__, __LINE__, type, tree, name)
#else
extern union option_value *get_opt_(struct option *, unsigned char *);
#define get_opt(tree, name, type) get_opt_(tree, name)
#endif

#define get_opt_bool_tree(tree, name)	get_opt(tree, name, OPT_BOOL)->number
#define get_opt_int_tree(tree, name)	get_opt(tree, name, OPT_INT)->number
#define get_opt_long_tree(tree, name)	get_opt(tree, name, OPT_LONG)->big_number
#define get_opt_str_tree(tree, name)	get_opt(tree, name, OPT_STRING)->string
#define get_opt_codepage_tree(tree, name)	get_opt(tree, name, OPT_CODEPAGE)->number
#define get_opt_color_tree(tree, name)	get_opt(tree, name, OPT_COLOR)->color
#define get_opt_tree_tree(tree_, name)	get_opt(tree_, name, OPT_TREE)->tree

#define get_opt_bool(name) get_opt_bool_tree(config_options, name)
#define get_opt_int(name) get_opt_int_tree(config_options, name)
#define get_opt_long(name) get_opt_long_tree(config_options, name)
#define get_opt_str(name) get_opt_str_tree(config_options, name)
#define get_opt_codepage(name) get_opt_codepage_tree(config_options, name)
#define get_opt_color(name) get_opt_color_tree(config_options, name)
#define get_opt_tree(name) get_opt_tree_tree(config_options, name)

#define get_cmd_opt_bool(name) get_opt_bool_tree(cmdline_options, name)
#define get_cmd_opt_int(name) get_opt_int_tree(cmdline_options, name)
#define get_cmd_opt_long(name) get_opt_long_tree(cmdline_options, name)
#define get_cmd_opt_str(name) get_opt_str_tree(cmdline_options, name)
#define get_cmd_opt_codepage(name) get_opt_codepage_tree(cmdline_options, name)
#define get_cmd_opt_color(name) get_opt_color_tree(cmdline_options, name)
#define get_cmd_opt_tree(name) get_opt_tree_tree(cmdline_options, name)

extern struct option *add_opt(struct option *, unsigned char *, unsigned char *,
			      unsigned char *, enum option_flags, enum option_type,
			      long, long, longptr_T, unsigned char *);

/* Hack which permit to disable option descriptions, to reduce elinks binary size.
 * It may of some use for people wanting a very small static non-i18n elinks binary,
 * at time of writing gain is over 25Kbytes. --Zas */
#ifndef CONFIG_SMALL
#define DESC(x) (x)
#else
#define DESC(x) ((unsigned char *) "")
#endif


#define add_opt_bool_tree(tree, path, capt, name, flags, def, desc) \
	add_opt(tree, path, capt, name, flags, OPT_BOOL, 0, 1, (longptr_T) def, DESC(desc))

#define add_opt_int_tree(tree, path, capt, name, flags, min, max, def, desc) \
	add_opt(tree, path, capt, name, flags, OPT_INT, min, max, (longptr_T) def, DESC(desc))

#define add_opt_long_tree(tree, path, capt, name, flags, min, max, def, desc) \
	add_opt(tree, path, capt, name, flags, OPT_LONG, min, max, (longptr_T) def, DESC(desc))

#define add_opt_str_tree(tree, path, capt, name, flags, def, desc) \
do { \
	unsigned char *ptr = mem_alloc(MAX_STR_LEN); \
	safe_strncpy(ptr, def, MAX_STR_LEN); \
	add_opt(tree, path, capt, name, flags, OPT_STRING, 0, MAX_STR_LEN, (longptr_T) ptr, DESC(desc)); \
} while (0)

#define add_opt_codepage_tree(tree, path, capt, name, flags, def, desc) \
	add_opt(tree, path, capt, name, flags, OPT_CODEPAGE, 0, 0, (longptr_T) get_cp_index(def), DESC(desc))

#define add_opt_lang_tree(tree, path, capt, name, flags, desc) \
	add_opt(tree, path, capt, name, flags, OPT_LANGUAGE, 0, 0, (longptr_T) 0, DESC(desc))

#define add_opt_color_tree(tree, path, capt, name, flags, def, desc) \
	add_opt(tree, path, capt, name, flags, OPT_COLOR, 0, 0, (longptr_T) def, DESC(desc))

#define add_opt_command_tree(tree, path, capt, name, flags, cmd, desc) \
	add_opt(tree, path, capt, name, flags, OPT_COMMAND, 0, 0, (longptr_T) cmd, DESC(desc));

#define add_opt_alias_tree(tree, path, capt, name, flags, def, desc) \
	add_opt(tree, path, capt, name, flags, OPT_ALIAS, 0, strlen(def), (longptr_T) def, DESC(desc))

#define add_opt_tree_tree(tree, path, capt, name, flags, desc) \
	add_opt(tree, path, capt, name, flags, OPT_TREE, 0, 0, (longptr_T) init_options_tree(), DESC(desc));


/* Builtin options */

struct option_info {
	struct option option;
	unsigned char *path;
};

extern void register_options(struct option_info info[], struct option *tree);
extern void unregister_options(struct option_info info[], struct option *tree);

#define NULL_OPTION_INFO \
	{ INIT_OPTION(NULL, 0, 0, 0, 0, NULL, NULL, NULL), NULL }

#define INIT_OPT_BOOL(path, capt, name, flags, def, desc) \
	{ INIT_OPTION(name, flags, OPT_BOOL, 0, 1, def, DESC(desc), capt), path }

#define INIT_OPT_INT(path, capt, name, flags, min, max, def, desc) \
	{ INIT_OPTION(name, flags, OPT_INT, min, max, def, DESC(desc), capt), path }

#define INIT_OPT_LONG(path, capt, name, flags, min, max, def, desc) \
	{ INIT_OPTION(name, flags, OPT_LONG, min, max, def, DESC(desc), capt), path }

#define INIT_OPT_STRING(path, capt, name, flags, def, desc) \
	{ INIT_OPTION(name, flags, OPT_STRING, 0, MAX_STR_LEN, def, DESC(desc), capt), path }

#define INIT_OPT_CODEPAGE(path, capt, name, flags, def, desc) \
	{ INIT_OPTION(name, flags, OPT_CODEPAGE, 0, 0, def, DESC(desc), capt), path }

#define INIT_OPT_COLOR(path, capt, name, flags, def, desc) \
	{ INIT_OPTION(name, flags, OPT_COLOR, 0, 0, def, DESC(desc), capt), path }

#define INIT_OPT_LANGUAGE(path, capt, name, flags, desc) \
	{ INIT_OPTION(name, flags, OPT_LANGUAGE, 0, 0, 0, DESC(desc), capt), path }

#define INIT_OPT_COMMAND(path, capt, name, flags, cmd, desc) \
	{ INIT_OPTION(name, flags, OPT_COMMAND, 0, 0, cmd, DESC(desc), capt), path }

#define INIT_OPT_CMDALIAS(path, capt, name, flags, def, desc) \
	{ INIT_OPTION(name, flags, OPT_ALIAS, 0, sizeof(def) - 1, def, DESC(desc), capt), path }

#define INIT_OPT_ALIAS(path, name, flags, def) \
	{ INIT_OPTION(name, flags, OPT_ALIAS, 0, sizeof(def) - 1, def, NULL, NULL), path }

#define INIT_OPT_TREE(path, capt, name, flags, desc) \
	{ INIT_OPTION(name, flags, OPT_TREE, 0, 0, NULL, DESC(desc), capt), path }


/* TODO: We need to do *something* with this ;). */

enum referer {
	REFERER_NONE,
	REFERER_SAME_URL,
	REFERER_FAKE,
	REFERER_TRUE,
};

enum verbose_level {
	VERBOSE_QUIET,
	VERBOSE_WARNINGS,
	VERBOSE_ALL,

	VERBOSE_LEVELS,
};

#endif
