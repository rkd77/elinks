#ifndef EL__CONFIG_KBDBIND_H
#define EL__CONFIG_KBDBIND_H

#include "config/options.h"
#include "main/event.h"
#include "main/object.h"
#include "terminal/terminal.h"
#include "util/string.h"

struct listbox_item;
struct module;

/* Used for holding enum <keymap>_action values. */
typedef long action_id_T;

enum keymap_id {
	KEYMAP_INVALID = -1,
	KEYMAP_MAIN,
	KEYMAP_EDIT,
	KEYMAP_MENU,
	KEYMAP_MAX
};

struct action {
	unsigned char *str;
	action_id_T num;
	enum keymap_id keymap_id;
	unsigned char *desc;
	unsigned int flags;
};

struct action_list {
	struct action *actions;
	int num_actions;
};
struct keymap {
	unsigned char *str;
	enum keymap_id keymap_id;
	unsigned char *desc;
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
	ACT_##map##_OFFSET_##action

enum main_action_offset {
#include "config/actions-main.inc"

	MAIN_ACTIONS,
};

enum edit_action_offset {
#include "config/actions-edit.inc"

	EDIT_ACTIONS
};

enum menu_action_offset {
#include "config/actions-menu.inc"

	MENU_ACTIONS
};

#undef	ACTION_
#define ACTION_(map, name, action, caption, flags)	\
	ACT_##map##_##action

enum main_action {
#include "config/actions-main.inc"
};

enum edit_action {
#include "config/actions-edit.inc"
};

enum menu_action {
#include "config/actions-menu.inc"
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

struct keybinding {
	OBJECT_HEAD(struct keybinding);

	enum keymap_id keymap_id;
	action_id_T action_id;
	struct term_event_keyboard kbd;
	int event;
	enum kbdbind_flags flags;
	struct listbox_item *box_item;
};


struct keybinding *add_keybinding(enum keymap_id keymap_id, action_id_T action_id, struct term_event_keyboard *kbd, int event);
int keybinding_exists(enum keymap_id keymap_id, struct term_event_keyboard *kbd, action_id_T *action_id);
void free_keybinding(struct keybinding *);

struct action *get_action(enum keymap_id keymap_id, action_id_T action_id);
unsigned char *get_action_name(enum keymap_id keymap_id, action_id_T action_id);
action_id_T get_action_from_string(enum keymap_id keymap_id, unsigned char *str);
unsigned char *get_action_name_from_keystroke(enum keymap_id keymap_id,
                                              const unsigned char *keystroke_str);

static inline unsigned int
action_is_anonymous_safe(enum keymap_id keymap_id, action_id_T action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && !(action->flags & ACTION_RESTRICT_ANONYMOUS);
}

static inline unsigned int
action_requires_view_state(enum keymap_id keymap_id, action_id_T action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_VIEW_STATE);
}

static inline unsigned int
action_requires_location(enum keymap_id keymap_id, action_id_T action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_LOCATION);
}

static inline unsigned int
action_prefix_is_link_number(enum keymap_id keymap_id, action_id_T action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_JUMP_TO_LINK);
}

static inline unsigned int
action_requires_link(enum keymap_id keymap_id, action_id_T action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_LINK);
}

static inline unsigned int
action_requires_form(enum keymap_id keymap_id, action_id_T action_id)
{
	struct action *action = get_action(keymap_id, action_id);

	return action && (action->flags & ACTION_REQUIRE_FORM);
}

term_event_key_T read_key(const unsigned char *);
unsigned char *get_keymap_name(enum keymap_id);

int parse_keystroke(const unsigned char *, struct term_event_keyboard *);
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

action_id_T kbd_action(enum keymap_id, struct term_event *, int *);
struct keybinding *kbd_ev_lookup(enum keymap_id, struct term_event_keyboard *kbd, int *);
struct keybinding *kbd_nm_lookup(enum keymap_id, unsigned char *);

int bind_do(unsigned char *, const unsigned char *, unsigned char *, int);
unsigned char *bind_act(unsigned char *, const unsigned char *);
void bind_config_string(struct string *);

#ifdef CONFIG_SCRIPTING
int bind_key_to_event_name(unsigned char *, const unsigned char *, unsigned char *,
			   unsigned char **);
#endif

void add_keystroke_action_to_string(struct string *string, action_id_T action_id, enum keymap_id keymap_id);
unsigned char *get_keystroke(action_id_T action_id, enum keymap_id keymap_id);

void add_actions_to_string(struct string *string, action_id_T actions[],
			   enum keymap_id keymap_id, struct terminal *term);

extern struct module kbdbind_module;

#endif
