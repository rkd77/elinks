/* Keybinding implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/event.h"
#include "main/module.h"
#include "terminal/kbd.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


/* Fix namespace clash on MacOS. */
#define table table_elinks

static const struct action_list action_table[KEYMAP_MAX];
static struct keymap keymap_table[KEYMAP_MAX];
static LIST_OF(struct keybinding) keymaps[KEYMAP_MAX];

static void add_default_keybindings(void);

static int
delete_keybinding(enum keymap_id keymap_id, struct term_event_keyboard *kbd)
{
	struct keybinding *keybinding;

	foreach (keybinding, keymaps[keymap_id]) {
		int was_default = 0;

		if (!kbd_key_is(&keybinding->kbd, kbd->key))
			continue;

		if (!kbd_modifier_is(&keybinding->kbd, kbd->modifier))
			continue;


		if (keybinding->flags & KBDB_DEFAULT_KEY) {
			keybinding->flags &= ~KBDB_DEFAULT_KEY;
			was_default = 1;
		}

		free_keybinding(keybinding);

		return 1 + was_default;
	}

	return 0;
}

static int keybinding_is_default(struct keybinding *keybinding);

struct keybinding *
add_keybinding(enum keymap_id keymap_id, action_id_T action_id,
	       struct term_event_keyboard *kbd, int event)
{
	struct keybinding *keybinding;
	struct listbox_item *root;
	int is_default;

	is_default = (delete_keybinding(keymap_id, kbd) == 2);

	keybinding = mem_calloc(1, sizeof(*keybinding));
	if (!keybinding) return NULL;

	keybinding->keymap_id = keymap_id;
	keybinding->action_id = action_id;
	copy_struct(&keybinding->kbd, kbd);
	keybinding->event = event;
	keybinding->flags = is_default * KBDB_DEFAULT_KEY;
	if (keybinding_is_default(keybinding))
		keybinding->flags |= KBDB_DEFAULT_BINDING;

	object_nolock(keybinding, "keybinding");
	add_to_list(keymaps[keymap_id], keybinding);

	if (action_id == ACT_MAIN_NONE) {
		/* We don't want such a listbox_item, do we? */
		return NULL; /* Or goto. */
	}

	root = get_keybinding_action_box_item(keymap_id, action_id);
	if (!root) {
		return NULL; /* Or goto ;-). */
	}
	keybinding->box_item = add_listbox_leaf(&keybinding_browser, root, keybinding);

	return keybinding;
}

void
free_keybinding(struct keybinding *keybinding)
{
	if (keybinding->box_item) {
		done_listbox_item(&keybinding_browser, keybinding->box_item);
		keybinding->box_item = NULL;
	}

#ifdef CONFIG_SCRIPTING
/* TODO: unref function must be implemented. This is part of bug 810. */
/*	if (keybinding->event != EVENT_NONE)
		scripting_unref(keybinding->event); */
#endif

	if (keybinding->flags & KBDB_DEFAULT_KEY) {
		/* We cannot just delete a default keybinding, instead we have
		 * to rebind it to ACT_MAIN_NONE so that it gets written so to the
		 * config file. */
		keybinding->flags &= ~KBDB_DEFAULT_BINDING;
		keybinding->action_id = ACT_MAIN_NONE;
		return;
	}

	del_from_list(keybinding);
	mem_free(keybinding);
}

int
keybinding_exists(enum keymap_id keymap_id, struct term_event_keyboard *kbd,
		  action_id_T *action_id)
{
	struct keybinding *keybinding;

	foreach (keybinding, keymaps[keymap_id]) {
		if (!kbd_key_is(&keybinding->kbd, kbd->key))
			continue;

		if (!kbd_modifier_is(&keybinding->kbd, kbd->modifier))
			continue;

		if (action_id) *action_id = keybinding->action_id;

		return 1;
	}

	return 0;
}


action_id_T
kbd_action(enum keymap_id keymap_id, struct term_event *ev, int *event)
{
	struct keybinding *keybinding;

	if (ev->ev != EVENT_KBD) return -1;

	keybinding = kbd_ev_lookup(keymap_id, &ev->info.keyboard, event);
	return keybinding ? keybinding->action_id : -1;
}

struct keybinding *
kbd_ev_lookup(enum keymap_id keymap_id, struct term_event_keyboard *kbd, int *event)
{
	struct keybinding *keybinding;

	foreach (keybinding, keymaps[keymap_id]) {
		if (!kbd_key_is(&keybinding->kbd, kbd->key))
			continue;

		if (!kbd_modifier_is(&keybinding->kbd, kbd->modifier))
			continue;

		if (keybinding->action_id == ACT_MAIN_SCRIPTING_FUNCTION && event)
			*event = keybinding->event;

		return keybinding;
	}

	return NULL;
}

static struct keybinding *
kbd_act_lookup(enum keymap_id keymap_id, action_id_T action_id)
{
	struct keybinding *keybinding;

	foreach (keybinding, keymaps[keymap_id]) {
		if (action_id != keybinding->action_id)
			continue;

		return keybinding;
	}

	return NULL;
}

struct keybinding *
kbd_nm_lookup(enum keymap_id keymap_id, unsigned char *name)
{
	action_id_T action_id = get_action_from_string(keymap_id, name);

	if (action_id < 0) return NULL;

	return kbd_act_lookup(keymap_id, action_id);
}

static struct keybinding *
kbd_stroke_lookup(enum keymap_id keymap_id, const unsigned char *keystroke_str)
{
	struct term_event_keyboard kbd;

	if (parse_keystroke(keystroke_str, &kbd) < 0)
		return NULL;

	return kbd_ev_lookup(keymap_id, &kbd, NULL);
}


static struct keymap keymap_table[] = {
	{ "main", KEYMAP_MAIN, N_("Main mapping") },
	{ "edit", KEYMAP_EDIT, N_("Edit mapping") },
	{ "menu", KEYMAP_MENU, N_("Menu mapping") },
};


/*
 * Config file helpers.
 */

static const struct action *
get_action_from_keystroke(enum keymap_id keymap_id,
                          const unsigned char *keystroke_str)
{
	struct keybinding *keybinding = kbd_stroke_lookup(keymap_id,
	                                                  keystroke_str);

	return keybinding ? get_action(keymap_id, keybinding->action_id) : NULL;
}

unsigned char *
get_action_name_from_keystroke(enum keymap_id keymap_id,
                               const unsigned char *keystroke_str)
{
	const struct action *action = get_action_from_keystroke(keymap_id,
								keystroke_str);

	return action ? action->str : NULL;
}

action_id_T
get_action_from_string(enum keymap_id keymap_id, unsigned char *str)
{
	const struct action *action;

	assert(keymap_id >= 0 && keymap_id < KEYMAP_MAX);

	for (action = action_table[keymap_id].actions; action->str; action++)
		if (!strcmp(action->str, str))
			return action->num;

	return -1;
}

const struct action *
get_action(enum keymap_id keymap_id, action_id_T action_id)
{
	assert(keymap_id >= 0 && keymap_id < KEYMAP_MAX);

	if (action_id >= 0 && action_id < action_table[keymap_id].num_actions)
		return &action_table[keymap_id].actions[action_id];

	return NULL;
}

unsigned char *
get_action_name(enum keymap_id keymap_id, action_id_T action_id)
{
	const struct action *action = get_action(keymap_id, action_id);

	return action ? action->str : NULL;
}

static unsigned char *
get_action_desc(enum keymap_id keymap_id, action_id_T action_id)
{
	const struct action *action = get_action(keymap_id, action_id);

	return action ? (action->desc ? action->desc : action->str)
	              : NULL;
}


static struct keymap *
get_keymap(enum keymap_id keymap_id)
{
	assert(keymap_id >= 0 && keymap_id < KEYMAP_MAX);

	return &keymap_table[keymap_id];
}

static enum keymap_id
get_keymap_id(unsigned char *keymap_str)
{
	enum keymap_id keymap_id;

	for (keymap_id = 0; keymap_id < KEYMAP_MAX; keymap_id++)
		if (!strcmp(keymap_table[keymap_id].str, keymap_str))
			return keymap_id;

	return KEYMAP_INVALID;
}

unsigned char *
get_keymap_name(enum keymap_id keymap_id)
{
	return get_keymap(keymap_id)->str;
}


struct named_key {
	const unsigned char *str;
	term_event_key_T num;
};

static const struct named_key key_table[] = {
	{ "Enter",	KBD_ENTER },
	{ "Space",	' ' },
	{ "Backspace",	KBD_BS },
	{ "Tab",	KBD_TAB },
	{ "Escape",	KBD_ESC },
	{ "Left",	KBD_LEFT },
	{ "Right",	KBD_RIGHT },
	{ "Up",		KBD_UP },
	{ "Down",	KBD_DOWN },
	{ "Insert",	KBD_INS },
	{ "Delete",	KBD_DEL },
	{ "Home",	KBD_HOME },
	{ "End",	KBD_END },
	{ "PageUp",	KBD_PAGE_UP },
	{ "PageDown",	KBD_PAGE_DOWN },
	{ "F1",		KBD_F1 },
	{ "F2",		KBD_F2 },
	{ "F3",		KBD_F3 },
	{ "F4",		KBD_F4 },
	{ "F5",		KBD_F5 },
	{ "F6",		KBD_F6 },
	{ "F7",		KBD_F7 },
	{ "F8",		KBD_F8 },
	{ "F9",		KBD_F9 },
	{ "F10",	KBD_F10 },
	{ "F11",	KBD_F11 },
	{ "F12",	KBD_F12 },
	{ NULL, KBD_UNDEF }
};

term_event_key_T
read_key(const unsigned char *key_str)
{
	const struct named_key *key;

	if (key_str[0] && !key_str[1])
		return key_str[0];

	for (key = key_table; key->str; key++)
		if (!c_strcasecmp(key->str, key_str))
			return key->num;

	return KBD_UNDEF;
}

/* Parse the string @s as the name of a keystroke.
 * Write the parsed key and modifiers to *@kbd.
 * Return >=0 on success, <0 on error.  */
int
parse_keystroke(const unsigned char *s, struct term_event_keyboard *kbd)
{
	kbd->modifier = KBD_MOD_NONE;
	while (1) {
		if (!c_strncasecmp(s, "Shift", 5) && (s[5] == '-' || s[5] == '+')) {
			/* Shift+a == shiFt-a == Shift-a */
			kbd->modifier |= KBD_MOD_SHIFT;
			s += 6;

		} else if (!c_strncasecmp(s, "Ctrl", 4) && (s[4] == '-' || s[4] == '+')) {
			/* Ctrl+a == ctRl-a == Ctrl-a */
			kbd->modifier |= KBD_MOD_CTRL;
			s += 5;

		} else if (!c_strncasecmp(s, "Alt", 3) && (s[3] == '-' || s[3] == '+')) {
			/* Alt+a == aLt-a == Alt-a */
			kbd->modifier |= KBD_MOD_ALT;
			s += 4;

		} else {
			/* No modifier. */
			break;
		}
	}

	kbd->key = read_key(s);

	if ((kbd->modifier & KBD_MOD_CTRL) != 0
	    && is_kbd_character(kbd->key) && kbd->key <= 0x7F) {
		/* Convert Ctrl-letter keystrokes to upper case,
		 * e.g. Ctrl-a to Ctrl-A.  Do this because terminals
		 * typically generate the same ASCII control
		 * characters regardless of Shift and Caps Lock.
		 *
		 * However, that applies only to ASCII letters.  For
		 * other Ctrl-letter combinations, there seems to be
		 * no standard of what the terminal should generate.
		 * Because it is possible that the terminal does not
		 * fold case then, ELinks should treat upper-case and
		 * lower-case variants of such keystrokes as different
		 * and let the user bind them to separate actions.
		 * Besides, toupper() might not understand the charset
		 * of kbd->key.
		 *
		 * FIXME: Actually, it is possible that some terminals
		 * preserve case in all Ctrl-letter keystrokes, even
		 * for ASCII letters.  In particular, the Win32
		 * console API looks like it might do this.  When the
		 * terminal handling part of ELinks is eventually
		 * extended to fully support that, it could be a good
		 * idea to change parse_keystroke() not to fold case,
		 * and instead make kbd_ev_lookup() or its callers
		 * search for different variants of the keystroke if
		 * the original one is not bound to any action.  */
		kbd->key = c_toupper(kbd->key);
	}

	return (kbd->key == KBD_UNDEF) ? -1 : 0;
}

void
add_keystroke_to_string(struct string *str, struct term_event_keyboard *kbd,
                        int escape)
{
	unsigned char key_buffer[3] = "\\x";
	const unsigned char *key_string = NULL;
	const struct named_key *key;

	if (kbd->key == KBD_UNDEF) return;

	if (kbd->modifier & KBD_MOD_SHIFT)
		add_to_string(str, "Shift-");
	if (kbd->modifier & KBD_MOD_CTRL)
		add_to_string(str, "Ctrl-");
	if (kbd->modifier & KBD_MOD_ALT)
		add_to_string(str, "Alt-");

	for (key = key_table; key->str; key++) {
		if (kbd->key == key->num) {
			key_string = key->str;
			break;
		}
	}

	if (!key_string) {
		key_string = key_buffer + 1;
		key_buffer[1] = (unsigned char) kbd->key;
		if (escape && strchr("'\"\\", kbd->key))
			key_string--;
	}

	add_to_string(str, key_string);
}

void
add_keystroke_action_to_string(struct string *string, action_id_T action_id,
                               enum keymap_id keymap_id)
{
	struct keybinding *keybinding = kbd_act_lookup(keymap_id, action_id);

	if (keybinding)
		add_keystroke_to_string(string, &keybinding->kbd, 0);
}

unsigned char *
get_keystroke(action_id_T action_id, enum keymap_id keymap_id)
{
	struct string keystroke;

	if (!init_string(&keystroke)) return NULL;

	add_keystroke_action_to_string(&keystroke, action_id, keymap_id);

	/* Never return empty string */
	if (!keystroke.length) done_string(&keystroke);

	return keystroke.source;
}

void
add_actions_to_string(struct string *string, action_id_T action_ids[],
		      enum keymap_id keymap_id, struct terminal *term)
{
	int i;

	assert(keymap_id >= 0 && keymap_id < KEYMAP_MAX);

	add_format_to_string(string, "%s:\n", _(keymap_table[keymap_id].desc, term));

	for (i = 0; action_ids[i] != ACT_MAIN_NONE; i++) {
		struct keybinding *keybinding = kbd_act_lookup(keymap_id, action_ids[i]);
		int keystrokelen = string->length;
		unsigned char *desc = get_action_desc(keymap_id, action_ids[i]);

		if (!keybinding) continue;

		add_char_to_string(string, '\n');
		add_keystroke_to_string(string, &keybinding->kbd, 0);
		keystrokelen = string->length - keystrokelen;
		add_xchar_to_string(string, ' ', int_max(15 - keystrokelen, 1));
		add_to_string(string, _(desc, term));
	}
}

#define ACTION_(map, name, action, caption, flags)	\
	{ name, ACT_##map##_##action, KEYMAP_ID, caption, flags }

#undef KEYMAP_ID
#define KEYMAP_ID KEYMAP_MAIN
static const struct action main_action_table[MAIN_ACTIONS + 1] = {
#include "config/actions-main.inc"
};

#undef KEYMAP_ID
#define KEYMAP_ID KEYMAP_EDIT
static const struct action edit_action_table[EDIT_ACTIONS + 1] = {
#include "config/actions-edit.inc"
};

#undef KEYMAP_ID
#define KEYMAP_ID KEYMAP_MENU
static const struct action menu_action_table[MENU_ACTIONS + 1] = {
#include "config/actions-menu.inc"
};

static const struct action_list action_table[KEYMAP_MAX] = {
	{ main_action_table, sizeof_array(main_action_table) },
	{ edit_action_table, sizeof_array(edit_action_table) },
	{ menu_action_table, sizeof_array(menu_action_table) },
};

#undef KEYMAP_ID
#undef ACTION_


static void
init_keymaps(struct module *xxx)
{
	enum keymap_id keymap_id;

	for (keymap_id = 0; keymap_id < KEYMAP_MAX; keymap_id++)
		init_list(keymaps[keymap_id]);

	init_keybinding_listboxes(keymap_table, action_table);
	add_default_keybindings();
}

static void
free_keymaps(struct module *xxx)
{
	enum keymap_id keymap_id;

	done_keybinding_listboxes();

	for (keymap_id = 0; keymap_id < KEYMAP_MAX; keymap_id++)
		free_list(keymaps[keymap_id]);
}


/*
 * Bind to Lua function.
 */

#ifdef CONFIG_SCRIPTING
static unsigned char *
bind_key_to_event(unsigned char *ckmap, const unsigned char *ckey, int event)
{
	struct term_event_keyboard kbd;
	action_id_T action_id;
	enum keymap_id keymap_id = get_keymap_id(ckmap);

	if (keymap_id == KEYMAP_INVALID)
		return gettext("Unrecognised keymap");

	if (parse_keystroke(ckey, &kbd) < 0)
		return gettext("Error parsing keystroke");

	action_id = get_action_from_string(keymap_id, " *scripting-function*");
	if (action_id < 0)
		return gettext("Unrecognised action (internal error)");

	add_keybinding(keymap_id, action_id, &kbd, event);

	return NULL;
}

int
bind_key_to_event_name(unsigned char *ckmap, const unsigned char *ckey,
		       unsigned char *event_name, unsigned char **err)
{
	int event_id;

	event_id = register_event(event_name);

	if (event_id == EVENT_NONE) {
		*err = gettext("Error registering event");
		return EVENT_NONE;
	}

	*err = bind_key_to_event(ckmap, ckey, event_id);

	return event_id;
}
#endif


/*
 * Default keybindings.
 */

struct default_kb {
	struct term_event_keyboard kbd;
	action_id_T action_id;
};

static struct default_kb default_main_keymap[] = {
	{ { ' ',	 KBD_MOD_NONE }, ACT_MAIN_MOVE_PAGE_DOWN },
	{ { '#',	 KBD_MOD_NONE }, ACT_MAIN_SEARCH_TYPEAHEAD },
	{ { '%',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_DOCUMENT_COLORS },
	{ { '*',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_DISPLAY_IMAGES },
	{ { ',',	 KBD_MOD_NONE }, ACT_MAIN_LUA_CONSOLE },
	{ { '.',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_NUMBERED_LINKS },
	{ { '/',	 KBD_MOD_NONE }, ACT_MAIN_SEARCH },
	{ { ':',	 KBD_MOD_NONE }, ACT_MAIN_EXMODE },
	{ { '<',	 KBD_MOD_NONE }, ACT_MAIN_TAB_PREV },
	{ { '<',	 KBD_MOD_ALT  }, ACT_MAIN_TAB_MOVE_LEFT },
	{ { '=',	 KBD_MOD_NONE }, ACT_MAIN_DOCUMENT_INFO },
	{ { '>',	 KBD_MOD_NONE }, ACT_MAIN_TAB_NEXT },
	{ { '>',	 KBD_MOD_ALT  }, ACT_MAIN_TAB_MOVE_RIGHT },
	{ { '?',	 KBD_MOD_NONE }, ACT_MAIN_SEARCH_BACK },
	{ { 'A',	 KBD_MOD_NONE }, ACT_MAIN_ADD_BOOKMARK_LINK },
	{ { 'A',	 KBD_MOD_CTRL }, ACT_MAIN_MOVE_DOCUMENT_START },
	{ { 'B',	 KBD_MOD_CTRL }, ACT_MAIN_MOVE_PAGE_UP },
	{ { 'C',	 KBD_MOD_NONE }, ACT_MAIN_CACHE_MANAGER },
	{ { 'D',	 KBD_MOD_NONE }, ACT_MAIN_DOWNLOAD_MANAGER },
	{ { 'E',	 KBD_MOD_NONE }, ACT_MAIN_GOTO_URL_CURRENT_LINK },
	{ { 'E',	 KBD_MOD_CTRL }, ACT_MAIN_MOVE_DOCUMENT_END },
	{ { 'F',	 KBD_MOD_NONE }, ACT_MAIN_FORMHIST_MANAGER },
	{ { 'F',	 KBD_MOD_CTRL }, ACT_MAIN_MOVE_PAGE_DOWN },
	{ { 'G',	 KBD_MOD_NONE }, ACT_MAIN_GOTO_URL_CURRENT },
	{ { 'H',	 KBD_MOD_NONE }, ACT_MAIN_GOTO_URL_HOME },
	{ { 'K',	 KBD_MOD_NONE }, ACT_MAIN_COOKIE_MANAGER },
	{ { 'K',	 KBD_MOD_CTRL }, ACT_MAIN_COOKIES_LOAD },
	{ { 'L',	 KBD_MOD_NONE }, ACT_MAIN_LINK_MENU },
	{ { 'L',	 KBD_MOD_CTRL }, ACT_MAIN_REDRAW },
	{ { 'N',	 KBD_MOD_NONE }, ACT_MAIN_FIND_NEXT_BACK },
	{ { 'N',	 KBD_MOD_CTRL }, ACT_MAIN_SCROLL_DOWN },
	{ { 'P',	 KBD_MOD_CTRL }, ACT_MAIN_SCROLL_UP },
	{ { 'Q',	 KBD_MOD_NONE }, ACT_MAIN_REALLY_QUIT },
	{ { 'R',	 KBD_MOD_CTRL }, ACT_MAIN_RELOAD },
	{ { 'T',	 KBD_MOD_NONE }, ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND },
	{ { 'W',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_WRAP_TEXT },
	{ { '[',	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_LEFT },
	{ { '\'',	 KBD_MOD_NONE }, ACT_MAIN_MARK_GOTO },
	{ { '\\',	 KBD_MOD_NONE }, ACT_MAIN_TOGGLE_HTML_PLAIN },
	{ { ']',	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_RIGHT },
	{ { 'a',	 KBD_MOD_NONE }, ACT_MAIN_ADD_BOOKMARK },
	{ { 'b',	 KBD_MOD_NONE }, ACT_MAIN_MOVE_PAGE_UP },
	{ { 'c',	 KBD_MOD_NONE }, ACT_MAIN_TAB_CLOSE },
	{ { 'd',	 KBD_MOD_NONE }, ACT_MAIN_LINK_DOWNLOAD },
	{ { 'e',	 KBD_MOD_NONE }, ACT_MAIN_TAB_MENU },
	{ { 'f',	 KBD_MOD_NONE }, ACT_MAIN_FRAME_MAXIMIZE },
	{ { 'g',	 KBD_MOD_NONE }, ACT_MAIN_GOTO_URL },
	{ { 'h',	 KBD_MOD_NONE }, ACT_MAIN_HISTORY_MANAGER },
	{ { 'k',	 KBD_MOD_NONE }, ACT_MAIN_KEYBINDING_MANAGER },
	{ { 'l',	 KBD_MOD_NONE }, ACT_MAIN_JUMP_TO_LINK },
	{ { 'm',	 KBD_MOD_NONE }, ACT_MAIN_MARK_SET },
	{ { 'n',	 KBD_MOD_NONE }, ACT_MAIN_FIND_NEXT },
	{ { 'o',	 KBD_MOD_NONE }, ACT_MAIN_OPTIONS_MANAGER },
	{ { 'q',	 KBD_MOD_NONE }, ACT_MAIN_QUIT },
	{ { 'r',	 KBD_MOD_NONE }, ACT_MAIN_LINK_DOWNLOAD_RESUME },
	{ { 's',	 KBD_MOD_NONE }, ACT_MAIN_BOOKMARK_MANAGER },
	{ { 't',	 KBD_MOD_NONE }, ACT_MAIN_OPEN_NEW_TAB },
	{ { 'u',	 KBD_MOD_NONE }, ACT_MAIN_HISTORY_MOVE_FORWARD },
	{ { 'v',	 KBD_MOD_NONE }, ACT_MAIN_VIEW_IMAGE },
	{ { 'x',	 KBD_MOD_NONE }, ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ { 'z',	 KBD_MOD_NONE }, ACT_MAIN_ABORT_CONNECTION },
	{ { '{',	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_LEFT },
	{ { '|',	 KBD_MOD_NONE }, ACT_MAIN_HEADER_INFO },
	{ { '}',	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_RIGHT },
	{ { KBD_BS,	 KBD_MOD_NONE }, ACT_MAIN_BACKSPACE_PREFIX },
	{ { KBD_DEL,	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_DOWN },
	{ { KBD_DOWN,	 KBD_MOD_NONE }, ACT_MAIN_MOVE_LINK_NEXT },
	{ { KBD_END,	 KBD_MOD_NONE }, ACT_MAIN_MOVE_DOCUMENT_END },
	{ { KBD_ENTER,	 KBD_MOD_NONE }, ACT_MAIN_LINK_FOLLOW },
	{ { KBD_ENTER,	 KBD_MOD_CTRL }, ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ { KBD_ESC,	 KBD_MOD_NONE }, ACT_MAIN_MENU },
	{ { KBD_F10,	 KBD_MOD_NONE }, ACT_MAIN_FILE_MENU },
	{ { KBD_F9,	 KBD_MOD_NONE }, ACT_MAIN_MENU },
	{ { KBD_HOME,	 KBD_MOD_NONE }, ACT_MAIN_MOVE_DOCUMENT_START },
	{ { KBD_INS,	 KBD_MOD_NONE }, ACT_MAIN_SCROLL_UP },
	{ { KBD_INS,	 KBD_MOD_CTRL }, ACT_MAIN_COPY_CLIPBOARD },
	{ { KBD_LEFT,	 KBD_MOD_NONE }, ACT_MAIN_HISTORY_MOVE_BACK },
	{ { KBD_PAGE_DOWN, KBD_MOD_NONE }, ACT_MAIN_MOVE_PAGE_DOWN },
	{ { KBD_PAGE_UP, KBD_MOD_NONE }, ACT_MAIN_MOVE_PAGE_UP },
	{ { KBD_RIGHT,	 KBD_MOD_NONE }, ACT_MAIN_LINK_FOLLOW },
	{ { KBD_RIGHT,	 KBD_MOD_CTRL }, ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ { KBD_TAB,	 KBD_MOD_NONE }, ACT_MAIN_FRAME_NEXT },
	{ { KBD_TAB,	 KBD_MOD_ALT  }, ACT_MAIN_FRAME_PREV },
	{ { KBD_TAB,     KBD_MOD_SHIFT}, ACT_MAIN_FRAME_PREV },
	{ { KBD_UP,	 KBD_MOD_NONE }, ACT_MAIN_MOVE_LINK_PREV },
	{ { 0, 0 }, 0 }
};

static struct default_kb default_edit_keymap[] = {
	{ { '<',	 KBD_MOD_ALT  }, ACT_EDIT_BEGINNING_OF_BUFFER },
	{ { '>',	 KBD_MOD_ALT  }, ACT_EDIT_END_OF_BUFFER },
	{ { 'A',	 KBD_MOD_CTRL }, ACT_EDIT_HOME },
	{ { 'b',	 KBD_MOD_ALT  }, ACT_EDIT_MOVE_BACKWARD_WORD },
	{ { 'D',	 KBD_MOD_CTRL }, ACT_EDIT_DELETE },
	{ { 'E',	 KBD_MOD_CTRL }, ACT_EDIT_END },
	{ { 'f',	 KBD_MOD_ALT  }, ACT_EDIT_MOVE_FORWARD_WORD },
	{ { 'H',	 KBD_MOD_CTRL }, ACT_EDIT_BACKSPACE },
	{ { 'K',	 KBD_MOD_CTRL }, ACT_EDIT_KILL_TO_EOL },
	{ { 'L',	 KBD_MOD_CTRL }, ACT_EDIT_REDRAW },
	{ { 'r',	 KBD_MOD_ALT  }, ACT_EDIT_SEARCH_TOGGLE_REGEX },
	{ { 'F',	 KBD_MOD_CTRL }, ACT_EDIT_AUTO_COMPLETE_FILE },
	{ { 'R',	 KBD_MOD_CTRL }, ACT_EDIT_AUTO_COMPLETE_UNAMBIGUOUS },
	{ { 'T',	 KBD_MOD_CTRL }, ACT_EDIT_OPEN_EXTERNAL },
	{ { 'U',	 KBD_MOD_CTRL }, ACT_EDIT_KILL_TO_BOL },
	{ { 'V',	 KBD_MOD_CTRL }, ACT_EDIT_PASTE_CLIPBOARD },
	{ { 'W',	 KBD_MOD_CTRL }, ACT_EDIT_AUTO_COMPLETE },
	{ { 'X',	 KBD_MOD_CTRL }, ACT_EDIT_CUT_CLIPBOARD },
	{ { KBD_BS,	 KBD_MOD_ALT  }, ACT_EDIT_KILL_WORD_BACK },
	{ { KBD_BS,	 KBD_MOD_NONE }, ACT_EDIT_BACKSPACE },
	{ { KBD_DEL,	 KBD_MOD_NONE }, ACT_EDIT_DELETE },
	{ { KBD_DOWN,	 KBD_MOD_NONE }, ACT_EDIT_DOWN },
	{ { KBD_END,	 KBD_MOD_NONE }, ACT_EDIT_END },
	{ { KBD_ENTER,	 KBD_MOD_NONE }, ACT_EDIT_ENTER },
	{ { KBD_ESC,	 KBD_MOD_NONE }, ACT_EDIT_CANCEL },
	{ { KBD_F4,	 KBD_MOD_NONE }, ACT_EDIT_OPEN_EXTERNAL },
	{ { KBD_HOME,	 KBD_MOD_NONE }, ACT_EDIT_HOME },
	{ { KBD_INS,	 KBD_MOD_CTRL }, ACT_EDIT_COPY_CLIPBOARD },
	{ { KBD_LEFT,	 KBD_MOD_NONE }, ACT_EDIT_LEFT },
	{ { KBD_RIGHT,	 KBD_MOD_NONE }, ACT_EDIT_RIGHT },
	{ { KBD_TAB,	 KBD_MOD_NONE }, ACT_EDIT_NEXT_ITEM },
	{ { KBD_TAB,	 KBD_MOD_ALT  }, ACT_EDIT_PREVIOUS_ITEM },
	{ { KBD_TAB,	 KBD_MOD_SHIFT}, ACT_EDIT_PREVIOUS_ITEM },
	{ { KBD_UP,	 KBD_MOD_NONE }, ACT_EDIT_UP },
	{ { 0, 0 }, 0 }
};

static struct default_kb default_menu_keymap[] = {
	{ { ' ',	 KBD_MOD_NONE }, ACT_MENU_SELECT },
	{ { '*',	 KBD_MOD_NONE }, ACT_MENU_MARK_ITEM },
	{ { '+',	 KBD_MOD_NONE }, ACT_MENU_EXPAND },
	{ { '-',	 KBD_MOD_NONE }, ACT_MENU_UNEXPAND },
	{ { '/',	 KBD_MOD_NONE }, ACT_MENU_SEARCH },
	{ { '=',	 KBD_MOD_NONE }, ACT_MENU_EXPAND },
	{ { 'A',	 KBD_MOD_CTRL }, ACT_MENU_HOME },
	{ { 'B',	 KBD_MOD_CTRL }, ACT_MENU_PAGE_UP },
	{ { 'E',	 KBD_MOD_CTRL }, ACT_MENU_END },
	{ { 'F',	 KBD_MOD_CTRL }, ACT_MENU_PAGE_DOWN },
	{ { 'L',	 KBD_MOD_CTRL }, ACT_MENU_REDRAW },
	{ { 'N',	 KBD_MOD_CTRL }, ACT_MENU_DOWN },
	{ { 'P',	 KBD_MOD_CTRL }, ACT_MENU_UP },
	{ { 'V',	 KBD_MOD_ALT  }, ACT_MENU_PAGE_UP },
	{ { 'V',	 KBD_MOD_CTRL }, ACT_MENU_PAGE_DOWN },
	{ { '[',	 KBD_MOD_NONE }, ACT_MENU_EXPAND },
	{ { ']',	 KBD_MOD_NONE }, ACT_MENU_UNEXPAND },
	{ { '_',	 KBD_MOD_NONE }, ACT_MENU_UNEXPAND },
	{ { KBD_DEL,	 KBD_MOD_NONE }, ACT_MENU_DELETE },
	{ { KBD_DOWN,	 KBD_MOD_NONE }, ACT_MENU_DOWN },
	{ { KBD_END,	 KBD_MOD_NONE }, ACT_MENU_END },
	{ { KBD_ENTER,	 KBD_MOD_NONE }, ACT_MENU_ENTER },
	{ { KBD_ESC,	 KBD_MOD_NONE }, ACT_MENU_CANCEL },
	{ { KBD_HOME,	 KBD_MOD_NONE }, ACT_MENU_HOME },
	{ { KBD_INS,	 KBD_MOD_NONE }, ACT_MENU_MARK_ITEM },
	{ { KBD_LEFT,	 KBD_MOD_NONE }, ACT_MENU_LEFT },
	{ { KBD_PAGE_DOWN, KBD_MOD_NONE }, ACT_MENU_PAGE_DOWN },
	{ { KBD_PAGE_UP, KBD_MOD_NONE }, ACT_MENU_PAGE_UP },
	{ { KBD_RIGHT,	 KBD_MOD_NONE }, ACT_MENU_RIGHT },
	{ { KBD_TAB,	 KBD_MOD_NONE }, ACT_MENU_NEXT_ITEM },
	{ { KBD_TAB,	 KBD_MOD_ALT  }, ACT_MENU_PREVIOUS_ITEM },
	{ { KBD_TAB,	 KBD_MOD_SHIFT}, ACT_MENU_PREVIOUS_ITEM },
	{ { KBD_UP,	 KBD_MOD_NONE }, ACT_MENU_UP },
	{ { 0, 0 }, 0}
};

static struct default_kb *default_keybindings[] = {
	default_main_keymap,
	default_edit_keymap,
	default_menu_keymap,
};

static int
keybinding_is_default(struct keybinding *keybinding)
{
	struct default_kb default_keybinding = {
		{
			keybinding->kbd.key,
			keybinding->kbd.modifier
		},
		keybinding->action_id
	};
	struct default_kb *pos;

	for (pos = default_keybindings[keybinding->keymap_id]; pos->kbd.key; pos++)
		if (!memcmp(&default_keybinding, pos, sizeof(default_keybinding)))
			return 1;

	return 0;
}

static void
add_default_keybindings(void)
{
	/* Maybe we shouldn't delete old keybindings. But on the other side, we
	 * can't trust clueless users what they'll push into sources modifying
	 * defaults, can we? ;)) */
	enum keymap_id keymap_id;

	for (keymap_id = 0; keymap_id < KEYMAP_MAX; keymap_id++) {
		struct default_kb *kb;

		for (kb = default_keybindings[keymap_id]; kb->kbd.key; kb++) {
			struct keybinding *keybinding;

			keybinding = add_keybinding(keymap_id, kb->action_id, &kb->kbd, EVENT_NONE);
			keybinding->flags |= KBDB_DEFAULT_KEY | KBDB_DEFAULT_BINDING;
		}
	}
}


/*
 * Config file tools.
 */

struct action_alias {
	const unsigned char *str;
	action_id_T action_id;
};

static const struct action_alias main_action_aliases[] = {
	{ "back",		ACT_MAIN_HISTORY_MOVE_BACK },
	{ "down",		ACT_MAIN_MOVE_LINK_NEXT },
	{ "download",		ACT_MAIN_LINK_DOWNLOAD },
	{ "download-image",	ACT_MAIN_LINK_DOWNLOAD_IMAGE },
	{ "end",		ACT_MAIN_MOVE_DOCUMENT_END },
	{ "enter",		ACT_MAIN_LINK_FOLLOW },
	{ "enter-reload",	ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ "home",		ACT_MAIN_MOVE_DOCUMENT_START },
	{ "next-frame",		ACT_MAIN_FRAME_NEXT },
	{ "page-down",		ACT_MAIN_MOVE_PAGE_DOWN },
	{ "page-up",		ACT_MAIN_MOVE_PAGE_UP },
	{ "previous-frame",	ACT_MAIN_FRAME_PREV },
	{ "resume-download",	ACT_MAIN_LINK_DOWNLOAD_RESUME },
	{ "unback",		ACT_MAIN_HISTORY_MOVE_FORWARD },
	{ "up",			ACT_MAIN_MOVE_LINK_PREV },
	{ "zoom-frame",		ACT_MAIN_FRAME_MAXIMIZE },

	{ NULL, 0 }
};

static const struct action_alias edit_action_aliases[] = {
	{ "edit",		ACT_EDIT_OPEN_EXTERNAL },

	{ NULL, 0 }
};

static const struct action_alias *action_aliases[KEYMAP_MAX] = {
	main_action_aliases,
	edit_action_aliases,
	NULL,
};

static action_id_T
get_aliased_action(enum keymap_id keymap_id, unsigned char *action_str)
{
	assert(keymap_id >= 0 && keymap_id < KEYMAP_MAX);

	if (action_aliases[keymap_id]) {
		const struct action_alias *alias;

		for (alias = action_aliases[keymap_id]; alias->str; alias++)
			if (!strcmp(alias->str, action_str))
				return alias->action_id;
	}

	return get_action_from_string(keymap_id, action_str);
}

/* Return 0 when ok, something strange otherwise. */
int
bind_do(unsigned char *keymap_str, const unsigned char *keystroke_str,
	unsigned char *action_str, int is_system_conf)
{
	enum keymap_id keymap_id;
	action_id_T action_id;
	struct term_event_keyboard kbd;
	struct keybinding *keybinding;

	keymap_id = get_keymap_id(keymap_str);
	if (keymap_id == KEYMAP_INVALID) return 1;

	if (parse_keystroke(keystroke_str, &kbd) < 0) return 2;

	action_id = get_aliased_action(keymap_id, action_str);
	if (action_id < 0) return 77 / 9 - 5;

	keybinding = add_keybinding(keymap_id, action_id, &kbd, EVENT_NONE);
	if (keybinding && is_system_conf)
		keybinding->flags |= KBDB_DEFAULT_KEY | KBDB_DEFAULT_BINDING;

	return 0;
}

unsigned char *
bind_act(unsigned char *keymap_str, const unsigned char *keystroke_str)
{
	enum keymap_id keymap_id;
	unsigned char *action;
	struct keybinding *keybinding;

	keymap_id = get_keymap_id(keymap_str);
	if (keymap_id == KEYMAP_INVALID)
		return NULL;

	keybinding = kbd_stroke_lookup(keymap_id, keystroke_str);
	if (!keybinding) return NULL;

	action = get_action_name(keymap_id, keybinding->action_id);
	if (!action)
		return NULL;

	keybinding->flags |= KBDB_WATERMARK;
	return straconcat("\"", action, "\"", (unsigned char *) NULL);
}

static void
single_bind_config_string(struct string *file, enum keymap_id keymap_id,
			  struct keybinding *keybinding)
{
	unsigned char *keymap_str = get_keymap_name(keymap_id);
	unsigned char *action_str = get_action_name(keymap_id, keybinding->action_id);

	if (!keymap_str || !action_str || action_str[0] == ' ')
		return;

	if (keybinding->flags & KBDB_WATERMARK) {
		keybinding->flags &= ~KBDB_WATERMARK;
		return;
	}

	/* TODO: Maybe we should use string.write.. */
	add_to_string(file, "bind \"");
	add_to_string(file, keymap_str);
	add_to_string(file, "\" \"");
	add_keystroke_to_string(file, &keybinding->kbd, 1);
	add_to_string(file, "\" = \"");
	add_to_string(file, action_str);
	add_char_to_string(file, '\"');
	add_char_to_string(file, '\n');
}

void
bind_config_string(struct string *file)
{
	enum keymap_id keymap_id;

	for (keymap_id = 0; keymap_id < KEYMAP_MAX; keymap_id++) {
		struct keybinding *keybinding;

		foreach (keybinding, keymaps[keymap_id]) {
			/* Don't save default keybindings that has not been
			 * deleted (rebound to none action) (Bug 337). */
			if (keybinding->flags & KBDB_DEFAULT_BINDING)
				continue;

			single_bind_config_string(file, keymap_id, keybinding);
		}
	}
}

struct module kbdbind_module = struct_module(
	/* Because this module is listed in main_modules rather than
	 * in builtin_modules, its name does not appear in the user
	 * interface and so need not be translatable.  */
	/* name: */		"Keyboard Bindings",
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_keymaps,
	/* done: */		free_keymaps
);
