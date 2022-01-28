#ifndef EL__CONFIG_KBDBIND_H
#define EL__CONFIG_KBDBIND_H

#include "config/options.h"
#include "main/event.h"
#include "main/object.h"
#include "terminal/terminal.h"
#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif

struct listbox_item;
struct module;

/* Used for holding enum <keymap>_action values. */
typedef long action_id_T;

typedef long keymap_id_T;

enum keymap_id {
	KEYMAP_INVALID = -1,
	KEYMAP_MAIN,
	KEYMAP_EDIT,
	KEYMAP_MENU,
	KEYMAP_MAX
};

struct action {
	char *str;
	action_id_T num;
	keymap_id_T keymap_id;
	char *desc;
	unsigned int flags;
};

struct action_list {
	const struct action *actions;
	int num_actions;
};
struct keymap {
	char *str;
	keymap_id_T keymap_id;
	char *desc;
};

enum action_flags {
	ACTION_RESTRICT_ANONYMOUS	=    (1 << 16),
	ACTION_REQUIRE_VIEW_STATE	=    (1 << 17),
	ACTION_REQUIRE_LOCATION		=    (1 << 18),
	ACTION_JUMP_TO_LINK		=    (1 << 19),
	ACTION_REQUIRE_LINK		=    (1 << 20),
	ACTION_REQUIRE_FORM		=    (1 << 21),
	ACTION_FLAGS_MASK		= (0xFF << 16),
};

/* Note: if you add anything here, please keep it in alphabetical order,
 * and also update the table action_table[] in kbdbind.c.  */

#define ACTION_(map, name, action, caption, flags)	\
	ACT_##map##_##action

enum main_action {
#include "config/actions-main.inc"

	MAIN_ACTIONS,
};

typedef long main_action_T;

enum edit_action {
#include "config/actions-edit.inc"

	EDIT_ACTIONS
};

typedef long edit_action_T;

enum menu_action {
#include "config/actions-menu.inc"

	MENU_ACTIONS
};

#undef	ACTION_

enum kbdbind_flags {
	KBDB_WATERMARK = 1,
	KBDB_TOUCHED = 2,

	/* Marks whether the binding has a key that is used
	 * by one of the default bindings. */
	KBDB_DEFAULT_KEY = 4,

	/* Marks whether the binding itself (the combination of key
	 * _and_ action) is default. */
	KBDB_DEFAULT_BINDING = 8,
};

typedef unsigned short kbdbind_flags_T;

struct keybinding {
	OBJECT_HEAD(struct keybinding);

	keymap_id_T keymap_id;
	action_id_T action_id;
	struct term_event_keyboard kbd;
	int event;
	kbdbind_flags_T flags;
	struct listbox_item *box_item;
};


struct keybinding *add_keybinding(keymap_id_T keymap_id, action_id_T action_id, struct term_event_keyboard *kbd, int event);
int keybinding_exists(keymap_id_T keymap_id, struct term_event_keyboard *kbd, action_id_T *action_id);
void free_keybinding(struct keybinding *);

const struct action *get_action(keymap_id_T keymap_id, action_id_T action_id);
char *get_action_name(keymap_id_T keymap_id, action_id_T action_id);
action_id_T get_action_from_string(keymap_id_T keymap_id, char *str);
char *get_action_name_from_keystroke(keymap_id_T keymap_id,
                                              const char *keystroke_str);

static inline unsigned int
action_is_anonymous_safe(keymap_id_T keymap_id, action_id_T action_id)
{
	const struct action *action = get_action(keymap_id, action_id);

	return action && !(action->flags & ACTION_RESTRICT_ANONYMOUS);
}

static inline unsigned int
action_requires_view_state(keymap_id_T keymap_id, action_id_T action_id)
{
	const struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_VIEW_STATE);
}

static inline unsigned int
action_requires_location(keymap_id_T keymap_id, action_id_T action_id)
{
	const struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_LOCATION);
}

static inline unsigned int
action_prefix_is_link_number(keymap_id_T keymap_id, action_id_T action_id)
{
	const struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_JUMP_TO_LINK);
}

static inline unsigned int
action_requires_link(keymap_id_T keymap_id, action_id_T action_id)
{
	const struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_LINK);
}

static inline unsigned int
action_requires_form(keymap_id_T keymap_id, action_id_T action_id)
{
	const struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_FORM);
}

term_event_key_T read_key(const char *);
char *get_keymap_name(keymap_id_T);

int parse_keystroke(const char *, struct term_event_keyboard *);
void add_keystroke_to_string(struct string *str, struct term_event_keyboard *kbd, int escape);

/* void add_accesskey_to_string(struct string *str, unicode_val_T accesskey); */
#define add_accesskey_to_string(str, accesskey) do { 		\
	struct term_event_keyboard kbd; 			\
	/* FIXME: #ifndef CONFIG_UTF8, kbd.key is encoded in	\
	 * the charset of the terminal, so accesskey should be	\
	 * converted from unicode_val_T to that.		\
	 * #ifdef CONFIG_UTF8, the code is correct.  */		\
	kbd.key = accesskey;					\
	/* try_document_key() recognizes only Alt-accesskey	\
	 * combos.  */						\
	kbd.modifier = KBD_MOD_ALT;				\
	add_keystroke_to_string(str, &kbd, 0); 			\
} while (0)

action_id_T kbd_action(keymap_id_T, struct term_event *, int *);
struct keybinding *kbd_ev_lookup(keymap_id_T, struct term_event_keyboard *kbd, int *);
struct keybinding *kbd_nm_lookup(keymap_id_T, char *);

int bind_do(char *, const char *, char *, int);
char *bind_act(char *, const char *);
void bind_config_string(struct string *);

#ifdef CONFIG_SCRIPTING
int bind_key_to_event_name(char *, const char *, char *,
			   char **);
#endif

void add_keystroke_action_to_string(struct string *string, action_id_T action_id, keymap_id_T keymap_id);
char *get_keystroke(action_id_T action_id, keymap_id_T keymap_id);

void add_actions_to_string(struct string *string, action_id_T actions[],
			   keymap_id_T keymap_id, struct terminal *term);

extern struct module kbdbind_module;

#ifdef __cplusplus
}
#endif

#endif
