/* $Id: kbdbind.h,v 1.136.6.1 2005/01/03 23:58:47 jonas Exp $ */

#ifndef EL__CONFIG_KBDBIND_H
#define EL__CONFIG_KBDBIND_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "config/options.h"
#include "terminal/terminal.h"
#include "util/object.h"
#include "util/string.h"

struct strtonum {
	unsigned char *str;
	long num;
	unsigned char *desc;
};

enum keymap {
	KEYMAP_MAIN,
	KEYMAP_EDIT,
	KEYMAP_MENU,
	KEYMAP_MAX
};

/* Note: if you add anything here, please keep it in alphabetical order,
 * and also update the table action_table[] in kbdbind.c.  */

enum main_action {
	/* These two actions are common over all keymaps: */
	ACT_MAIN_NONE = 0,
	ACT_MAIN_SCRIPTING_FUNCTION,

	ACT_MAIN_ABORT_CONNECTION,
	ACT_MAIN_ADD_BOOKMARK,
	ACT_MAIN_ADD_BOOKMARK_LINK,
	ACT_MAIN_ADD_BOOKMARK_TABS,
	ACT_MAIN_AUTH_MANAGER,
	ACT_MAIN_BOOKMARK_MANAGER,
	ACT_MAIN_CACHE_MANAGER,
	ACT_MAIN_CACHE_MINIMIZE,
	ACT_MAIN_COOKIES_LOAD,
	ACT_MAIN_COOKIE_MANAGER,
	ACT_MAIN_COPY_CLIPBOARD,
	ACT_MAIN_DOCUMENT_INFO,
	ACT_MAIN_DOWNLOAD_MANAGER,
	ACT_MAIN_EXMODE,
	ACT_MAIN_FILE_MENU,
	ACT_MAIN_FIND_NEXT,
	ACT_MAIN_FIND_NEXT_BACK,
	ACT_MAIN_FORGET_CREDENTIALS,
	ACT_MAIN_FORMHIST_MANAGER,
	ACT_MAIN_FRAME_EXTERNAL_COMMAND,
	ACT_MAIN_FRAME_MAXIMIZE,
	ACT_MAIN_FRAME_NEXT,
	ACT_MAIN_FRAME_PREV,
	ACT_MAIN_GOTO_URL,
	ACT_MAIN_GOTO_URL_CURRENT,
	ACT_MAIN_GOTO_URL_CURRENT_LINK,
	ACT_MAIN_GOTO_URL_HOME,
	ACT_MAIN_HEADER_INFO,
	ACT_MAIN_HISTORY_MANAGER,
	ACT_MAIN_HISTORY_MOVE_BACK,
	ACT_MAIN_HISTORY_MOVE_FORWARD,
	ACT_MAIN_JUMP_TO_LINK,
	ACT_MAIN_KEYBINDING_MANAGER,
	ACT_MAIN_KILL_BACKGROUNDED_CONNECTIONS,
	ACT_MAIN_LINK_DOWNLOAD,
	ACT_MAIN_LINK_DOWNLOAD_IMAGE,
	ACT_MAIN_LINK_DOWNLOAD_RESUME,
	ACT_MAIN_LINK_EXTERNAL_COMMAND,
	ACT_MAIN_LINK_FOLLOW,
	ACT_MAIN_LINK_FOLLOW_RELOAD,
	ACT_MAIN_LINK_MENU,
	ACT_MAIN_LUA_CONSOLE,
	ACT_MAIN_MARK_GOTO,
	ACT_MAIN_MARK_SET,
	ACT_MAIN_MENU,
	ACT_MAIN_MOVE_CURSOR_DOWN,
	ACT_MAIN_MOVE_CURSOR_LEFT,
	ACT_MAIN_MOVE_CURSOR_RIGHT,
	ACT_MAIN_MOVE_CURSOR_UP,
	ACT_MAIN_MOVE_DOCUMENT_END,
	ACT_MAIN_MOVE_DOCUMENT_START,
	ACT_MAIN_MOVE_LINK_DOWN,
	ACT_MAIN_MOVE_LINK_LEFT,
	ACT_MAIN_MOVE_LINK_NEXT,
	ACT_MAIN_MOVE_LINK_PREV,
	ACT_MAIN_MOVE_LINK_RIGHT,
	ACT_MAIN_MOVE_LINK_UP,
	ACT_MAIN_MOVE_PAGE_DOWN,
	ACT_MAIN_MOVE_PAGE_UP,
	ACT_MAIN_OPEN_LINK_IN_NEW_TAB,
	ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND,
	ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW,
	ACT_MAIN_OPEN_NEW_TAB,
	ACT_MAIN_OPEN_NEW_TAB_IN_BACKGROUND,
	ACT_MAIN_OPEN_NEW_WINDOW,
	ACT_MAIN_OPEN_OS_SHELL,
	ACT_MAIN_OPTIONS_MANAGER,
	ACT_MAIN_QUIT,
	ACT_MAIN_REALLY_QUIT,
	ACT_MAIN_REDRAW,
	ACT_MAIN_RELOAD,
	ACT_MAIN_RERENDER,
	ACT_MAIN_RESET_FORM,
	ACT_MAIN_RESOURCE_INFO,
	ACT_MAIN_SAVE_AS,
	ACT_MAIN_SAVE_FORMATTED,
	ACT_MAIN_SAVE_OPTIONS,
	ACT_MAIN_SAVE_URL_AS,
	ACT_MAIN_SCROLL_DOWN,
	ACT_MAIN_SCROLL_LEFT,
	ACT_MAIN_SCROLL_RIGHT,
	ACT_MAIN_SCROLL_UP,
	ACT_MAIN_SEARCH,
	ACT_MAIN_SEARCH_BACK,
	ACT_MAIN_SEARCH_TYPEAHEAD,
	ACT_MAIN_SEARCH_TYPEAHEAD_LINK,
	ACT_MAIN_SEARCH_TYPEAHEAD_TEXT,
	ACT_MAIN_SEARCH_TYPEAHEAD_TEXT_BACK,
	ACT_MAIN_SHOW_TERM_OPTIONS,
	ACT_MAIN_SUBMIT_FORM,
	ACT_MAIN_SUBMIT_FORM_RELOAD,
	ACT_MAIN_TAB_CLOSE,
	ACT_MAIN_TAB_CLOSE_ALL_BUT_CURRENT,
	ACT_MAIN_TAB_EXTERNAL_COMMAND,
	ACT_MAIN_TAB_MENU,
	ACT_MAIN_TAB_MOVE_LEFT,
	ACT_MAIN_TAB_MOVE_RIGHT,
	ACT_MAIN_TAB_NEXT,
	ACT_MAIN_TAB_PREV,
	ACT_MAIN_TERMINAL_RESIZE,
	ACT_MAIN_TOGGLE_CSS,
	ACT_MAIN_TOGGLE_DISPLAY_IMAGES,
	ACT_MAIN_TOGGLE_DISPLAY_TABLES,
	ACT_MAIN_TOGGLE_DOCUMENT_COLORS,
	ACT_MAIN_TOGGLE_HTML_PLAIN,
	ACT_MAIN_TOGGLE_NUMBERED_LINKS,
	ACT_MAIN_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES,
	ACT_MAIN_TOGGLE_WRAP_TEXT,
	ACT_MAIN_VIEW_IMAGE,

	MAIN_ACTIONS
};

enum edit_action {
	/* These two actions are common over all keymaps: */
	ACT_EDIT_NONE = 0,
	ACT_EDIT_SCRIPTING_FUNCTION,

	ACT_EDIT_AUTO_COMPLETE,
	ACT_EDIT_AUTO_COMPLETE_UNAMBIGUOUS,
	ACT_EDIT_BACKSPACE,
	ACT_EDIT_BEGINNING_OF_BUFFER,
	ACT_EDIT_CANCEL,
	ACT_EDIT_COPY_CLIPBOARD,
	ACT_EDIT_CUT_CLIPBOARD,
	ACT_EDIT_DELETE,
	ACT_EDIT_DOWN,
	ACT_EDIT_END,
	ACT_EDIT_END_OF_BUFFER,
	ACT_EDIT_ENTER,
	ACT_EDIT_HOME,
	ACT_EDIT_KILL_TO_BOL,
	ACT_EDIT_KILL_TO_EOL,
	ACT_EDIT_LEFT,
	ACT_EDIT_NEXT_ITEM,
	ACT_EDIT_OPEN_EXTERNAL,
	ACT_EDIT_PASTE_CLIPBOARD,
	ACT_EDIT_PREVIOUS_ITEM,
	ACT_EDIT_REDRAW,
	ACT_EDIT_RIGHT,
	ACT_EDIT_SEARCH_TOGGLE_REGEX,
	ACT_EDIT_UP,

	EDIT_ACTIONS
};

enum menu_action {
	/* These two actions are common over all keymaps: */
	ACT_MENU_NONE = 0,
	ACT_MENU_SCRIPTING_FUNCTION,

	ACT_MENU_CANCEL,
	ACT_MENU_DELETE,
	ACT_MENU_DOWN,
	ACT_MENU_END,
	ACT_MENU_ENTER,
	ACT_MENU_EXPAND,
	ACT_MENU_HOME,
	ACT_MENU_LEFT,
	ACT_MENU_MARK_ITEM,
	ACT_MENU_NEXT_ITEM,
	ACT_MENU_PAGE_DOWN,
	ACT_MENU_PAGE_UP,
	ACT_MENU_PREVIOUS_ITEM,
	ACT_MENU_REDRAW,
	ACT_MENU_RIGHT,
	ACT_MENU_SEARCH,
	ACT_MENU_SELECT,
	ACT_MENU_UNEXPAND,
	ACT_MENU_UP,

	MENU_ACTIONS
};

enum kbdbind_flags {
	KBDB_WATERMARK = 1,
	KBDB_TOUCHED = 2,
	/* Marks wether the keybinding combination also ``belongs'' to a
	 * default keybinding. */
	KBDB_DEFAULT = 4,
};

struct keybinding {
	LIST_HEAD(struct keybinding);
	enum keymap keymap;
	int action;
	long key;
	long meta;
	int func_ref;
	int flags;
	struct listbox_item *box_item;
	struct object object;
};


void init_keymaps(void);
void free_keymaps(void);

struct keybinding *add_keybinding(enum keymap km, int action, long key, long meta, int func_ref);
int keybinding_exists(enum keymap km, long key, long meta, int *action);
void free_keybinding(struct keybinding *);

long read_key(unsigned char *);
int read_action(enum keymap keymap, unsigned char *action);
unsigned char *write_action(enum keymap, int);
unsigned char *write_keymap(enum keymap);

int parse_keystroke(unsigned char *, long *, long *);
void make_keystroke(struct string *, long, long, int);

int kbd_action(enum keymap, struct term_event *, int *);
struct keybinding *kbd_ev_lookup(enum keymap, long, long, int *);
struct keybinding *kbd_nm_lookup(enum keymap, unsigned char *, int *);

int bind_do(unsigned char *, unsigned char *, unsigned char *);
unsigned char *bind_act(unsigned char *, unsigned char *);
void bind_config_string(struct string *);

#ifdef CONFIG_SCRIPTING
unsigned char *bind_scripting_func(unsigned char *, unsigned char *, int);
#endif

void add_keystroke_to_string(struct string *string, int action, enum keymap map);
unsigned char *get_keystroke(int action, enum keymap map);

void add_actions_to_string(struct string *string, int *actions,
			   enum keymap map, struct terminal *term);

#endif
