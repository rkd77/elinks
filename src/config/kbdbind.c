/* Keybinding implementation */
/* $Id: kbdbind.c,v 1.261.4.5 2005/05/01 22:05:10 jonas Exp $ */

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
#include "sched/event.h"
#include "terminal/kbd.h"
#include "util/memory.h"
#include "util/string.h"


/* Fix namespace clash on MacOS. */
#define table table_elinks

static struct strtonum *action_table[KEYMAP_MAX];
static struct list_head keymaps[KEYMAP_MAX];

static void add_default_keybindings(void);

static int
delete_keybinding(enum keymap km, long key, long meta)
{
	struct keybinding *kb;

	foreach (kb, keymaps[km]) {
		int was_default = 0;

		if (kb->key != key || kb->meta != meta)
			continue;

		if (kb->flags & KBDB_DEFAULT) {
			kb->flags &= ~KBDB_DEFAULT;
			was_default = 1;
		}

		free_keybinding(kb);

		return 1 + was_default;
	}

	return 0;
}

struct keybinding *
add_keybinding(enum keymap km, int action, long key, long meta, int func_ref)
{
	struct keybinding *kb;
	struct listbox_item *root;
	int is_default;

	is_default = delete_keybinding(km, key, meta) == 2;

	kb = mem_calloc(1, sizeof(*kb));
	if (!kb) return NULL;

	kb->keymap = km;
	kb->action = action;
	kb->key = key;
	kb->meta = meta;
	kb->func_ref = func_ref;
	kb->flags = is_default * KBDB_DEFAULT;

	object_nolock(kb, "keybinding");
	add_to_list(keymaps[km], kb);

	if (action == ACT_MAIN_NONE) {
		/* We don't want such a listbox_item, do we? */
		return NULL; /* Or goto. */
	}

	root = get_keybinding_action_box_item(km, action);
	if (!root) {
		return NULL; /* Or goto ;-). */
	}
	kb->box_item = add_listbox_leaf(&keybinding_browser, root, kb);

	return kb;
}

void
free_keybinding(struct keybinding *kb)
{
	if (kb->box_item) {
		done_listbox_item(&keybinding_browser, kb->box_item);
		kb->box_item = NULL;
	}

#ifdef CONFIG_SCRIPTING
/* TODO: unref function must be implemented. */
/*	if (kb->func_ref != EVENT_NONE)
		scripting_unref(kb->func_ref); */
#endif

	if (kb->flags & KBDB_DEFAULT) {
		/* We cannot just delete a default keybinding, instead we have
		 * to rebind it to ACT_MAIN_NONE so that it gets written so to the
		 * config file. */
		kb->action = ACT_MAIN_NONE;
		return;
	}

	del_from_list(kb);
	mem_free(kb);
}

int
keybinding_exists(enum keymap km, long key, long meta, int *action)
{
	struct keybinding *kb;

	foreach (kb, keymaps[km]) {
		if (kb->key != key || kb->meta != meta)
			continue;

		if (action) *action = kb->action;

		return 1;
	}

	return 0;
}


int
kbd_action(enum keymap kmap, struct term_event *ev, int *func_ref)
{
	struct keybinding *kb;

	if (ev->ev != EVENT_KBD) return -1;

	kb = kbd_ev_lookup(kmap, get_kbd_key(ev), get_kbd_modifier(ev), func_ref);
	return kb ? kb->action : -1;
}

struct keybinding *
kbd_ev_lookup(enum keymap kmap, long key, long meta, int *func_ref)
{
	struct keybinding *kb;

	foreach (kb, keymaps[kmap]) {
		if (key != kb->key || meta != kb->meta)
			continue;

		if (kb->action == ACT_MAIN_SCRIPTING_FUNCTION && func_ref)
			*func_ref = kb->func_ref;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_nm_lookup(enum keymap kmap, unsigned char *name, int *func_ref)
{
	struct keybinding *kb;
	int act = read_action(kmap, name);

	if (act < 0) return NULL;

	foreach (kb, keymaps[kmap]) {
		if (act != kb->action)
			continue;

		if (kb->action == ACT_MAIN_SCRIPTING_FUNCTION && func_ref)
			*func_ref = kb->func_ref;

		return kb;
	}

	return NULL;
}

struct keybinding *
kbd_act_lookup(enum keymap map, int action)
{
	struct keybinding *kb;

	foreach (kb, keymaps[map]) {
		if (action != kb->action)
			continue;

		return kb;
	}

	return NULL;
}

/*
 * Config file helpers.
 */

static long
strtonum(struct strtonum *table, unsigned char *str)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (!strcmp(rec->str, str))
			return rec->num;

	return -1;
}

static long
strcasetonum(struct strtonum *table, unsigned char *str)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (!strcasecmp(rec->str, str))
			return rec->num;

	return -1;
}

static unsigned char *
numtostr(struct strtonum *table, long num)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (num == rec->num)
			return rec->str;

	return NULL;
}


static unsigned char *
numtodesc(struct strtonum *table, long num)
{
	struct strtonum *rec;

	for (rec = table; rec->str; rec++)
		if (num == rec->num)
			return (rec->desc) ? rec->desc : rec->str;

	return NULL;
}


static struct strtonum keymap_table[] = {
	{ "main", KEYMAP_MAIN, N_("Main mapping") },
	{ "edit", KEYMAP_EDIT, N_("Edit mapping") },
	{ "menu", KEYMAP_MENU, N_("Menu mapping") },
	{ NULL, 0, NULL }
};

static int
read_keymap(unsigned char *keymap)
{
	return strtonum(keymap_table, keymap);
}

unsigned char *
write_keymap(enum keymap keymap)
{
	return numtostr(keymap_table, keymap);
}


static struct strtonum key_table[] = {
	{ "Enter", KBD_ENTER },
	{ "Space", ' ' },
	{ "Backspace", KBD_BS },
	{ "Tab", KBD_TAB },
	{ "Escape", KBD_ESC },
	{ "Left", KBD_LEFT },
	{ "Right", KBD_RIGHT },
	{ "Up", KBD_UP },
	{ "Down", KBD_DOWN },
	{ "Insert", KBD_INS },
	{ "Delete", KBD_DEL },
	{ "Home", KBD_HOME },
	{ "End", KBD_END },
	{ "PageUp", KBD_PAGE_UP },
	{ "PageDown", KBD_PAGE_DOWN },
	{ "F1", KBD_F1 },
	{ "F2", KBD_F2 },
	{ "F3", KBD_F3 },
	{ "F4", KBD_F4 },
	{ "F5", KBD_F5 },
	{ "F6", KBD_F6 },
	{ "F7", KBD_F7 },
	{ "F8", KBD_F8 },
	{ "F9", KBD_F9 },
	{ "F10", KBD_F10 },
	{ "F11", KBD_F11 },
	{ "F12", KBD_F12 },
	{ NULL, 0 }
};

long
read_key(unsigned char *key)
{
	return (key[0] && !key[1]) ? *key : strcasetonum(key_table, key);
}

int
parse_keystroke(unsigned char *s, long *key, long *meta)
{
	if (!strncasecmp(s, "Shift", 5) && (s[5] == '-' || s[5] == '+')) {
		/* Shift+a == shiFt-a == Shift-a */
		memcpy(s, "Shift-", 6);
		*meta = KBD_SHIFT;
		s += 6;

	} else if (!strncasecmp(s, "Ctrl", 4) && (s[4] == '-' || s[4] == '+')) {
		/* Ctrl+a == ctRl-a == Ctrl-a */
		memcpy(s, "Ctrl-", 5);
		*meta = KBD_CTRL;
		s += 5;
		/* Ctrl-a == Ctrl-A */
		if (s[0] && !s[1]) s[0] = toupper(s[0]);

	} else if (!strncasecmp(s, "Alt", 3) && (s[3] == '-' || s[3] == '+')) {
		/* Alt+a == aLt-a == Alt-a */
		memcpy(s, "Alt-", 4);
		*meta = KBD_ALT;
		s += 4;

	} else {
		/* No modifier. */
		*meta = 0;
	}

	*key = read_key(s);
	return (*key < 0) ? -1 : 0;
}

void
make_keystroke(struct string *str, long key, long meta, int escape)
{
	unsigned char key_buffer[3] = "\\x";
	unsigned char *key_string;

	if (key < 0) return;
	
	if (meta & KBD_SHIFT)
		add_to_string(str, "Shift-");
	if (meta & KBD_CTRL)
		add_to_string(str, "Ctrl-");
	if (meta & KBD_ALT)
		add_to_string(str, "Alt-");

	key_string = numtostr(key_table, key);
	if (!key_string) {
		key_string = key_buffer + 1;
		*key_string = (unsigned char) key;
		if (key == '\\' && escape)
			key_string--;
	}

	add_to_string(str, key_string);
}

void
add_keystroke_to_string(struct string *string, int action,
			enum keymap map)
{
	struct keybinding *kb = kbd_act_lookup(map, action);

	if (kb)
		make_keystroke(string, kb->key, kb->meta, 0);
}

unsigned char *
get_keystroke(int action, enum keymap map)
{
	struct string keystroke;

	if (!init_string(&keystroke)) return NULL;

	add_keystroke_to_string(&keystroke, action, map);

	/* Never return empty string */
	if (!keystroke.length) done_string(&keystroke);

	return keystroke.source;
}

void
add_actions_to_string(struct string *string, int *actions,
		      enum keymap map, struct terminal *term)
{
	int i;

	assert(map >= 0 && map < KEYMAP_MAX);

	add_format_to_string(string, "%s:\n", _(numtodesc(keymap_table, map), term));

	for (i = 0; actions[i] != ACT_MAIN_NONE; i++) {
		struct keybinding *kb = kbd_act_lookup(map, actions[i]);
		int keystrokelen = string->length;
		unsigned char *desc = numtodesc(action_table[map], actions[i]);

		if (!kb) continue;

		add_char_to_string(string, '\n');
		make_keystroke(string, kb->key, kb->meta, 0);
		keystrokelen = string->length - keystrokelen;
		add_xchar_to_string(string, ' ', int_max(15 - keystrokelen, 1));
		add_to_string(string, _(desc, term));
	}
}

/* XXX: don't declare DACT(x) to (N_(x)) as i tried to do, it will break
 * gettextization. --Zas */
#ifndef CONFIG_SMALL
#define DACT(x) (x)
#else
#define DACT(x) (NULL)
#endif

/* Please keep these tables in alphabetical order, and in sync with
 * the ACT_* constants in kbdbind.h.  */

static struct strtonum main_action_table[MAIN_ACTIONS + 1] = {
	{ "none", ACT_MAIN_NONE, DACT(N_("Do nothing")) },
	{ " *scripting-function*", ACT_MAIN_SCRIPTING_FUNCTION, NULL }, /* internal use only */
	{ "abort-connection", ACT_MAIN_ABORT_CONNECTION, DACT(N_("Abort connection")) },
	{ "add-bookmark", ACT_MAIN_ADD_BOOKMARK, DACT(N_("Add a new bookmark")) },
	{ "add-bookmark-link", ACT_MAIN_ADD_BOOKMARK_LINK, DACT(N_("Add a new bookmark using current link")) },
	{ "add-bookmark-tabs", ACT_MAIN_ADD_BOOKMARK_TABS, DACT(N_("Bookmark all open tabs")) },
	{ "auth-manager", ACT_MAIN_AUTH_MANAGER, DACT(N_("Open authentication manager")) },
	{ "bookmark-manager", ACT_MAIN_BOOKMARK_MANAGER, DACT(N_("Open bookmark manager")) },
	{ "cache-manager", ACT_MAIN_CACHE_MANAGER, DACT(N_("Open cache manager")) },
	{ "cache-minimize", ACT_MAIN_CACHE_MINIMIZE, DACT(N_("Free unused cache entries")) },
	{ "cookie-manager", ACT_MAIN_COOKIE_MANAGER, DACT(N_("Open cookie manager")) },
	{ "cookies-load", ACT_MAIN_COOKIES_LOAD, DACT(N_("Reload cookies file")) },
	{ "copy-clipboard", ACT_MAIN_COPY_CLIPBOARD, DACT(N_("Copy text to clipboard")) },
	{ "document-info", ACT_MAIN_DOCUMENT_INFO, DACT(N_("Show information about the current page")) },
	{ "download-manager", ACT_MAIN_DOWNLOAD_MANAGER, DACT(N_("Open download manager")) },
	{ "exmode", ACT_MAIN_EXMODE, DACT(N_("Enter ex-mode (command line)")) },
	{ "file-menu", ACT_MAIN_FILE_MENU, DACT(N_("Open the File menu")) },
	{ "find-next", ACT_MAIN_FIND_NEXT, DACT(N_("Find the next occurrence of the current search text")) },
	{ "find-next-back", ACT_MAIN_FIND_NEXT_BACK, DACT(N_("Find the previous occurrence of the current search text")) },
	{ "forget-credentials", ACT_MAIN_FORGET_CREDENTIALS, DACT(N_("Forget authentication credentials")) },
	{ "formhist-manager", ACT_MAIN_FORMHIST_MANAGER, DACT(N_("Open form history manager")) },
	{ "frame-external-command", ACT_MAIN_FRAME_EXTERNAL_COMMAND, DACT(N_("Pass URI of current frame to external command")) },
	{ "frame-maximize", ACT_MAIN_FRAME_MAXIMIZE, DACT(N_("Maximize the current frame")) },
	{ "frame-next", ACT_MAIN_FRAME_NEXT, DACT(N_("Move to the next frame")) },
	{ "frame-prev", ACT_MAIN_FRAME_PREV, DACT(N_("Move to the previous frame")) },
	{ "goto-url", ACT_MAIN_GOTO_URL, DACT(N_("Open \"Go to URL\" dialog box")) },
	{ "goto-url-current", ACT_MAIN_GOTO_URL_CURRENT, DACT(N_("Open \"Go to URL\" dialog box containing the current URL")) },
	{ "goto-url-current-link", ACT_MAIN_GOTO_URL_CURRENT_LINK, DACT(N_("Open \"Go to URL\" dialog box containing the current link URL")) },
	{ "goto-url-home", ACT_MAIN_GOTO_URL_HOME, DACT(N_("Go to the homepage")) },
	{ "header-info", ACT_MAIN_HEADER_INFO, DACT(N_("Show information about the current page protocol headers")) },
	{ "history-manager", ACT_MAIN_HISTORY_MANAGER, DACT(N_("Open history manager")) },
	{ "history-move-back", ACT_MAIN_HISTORY_MOVE_BACK, DACT(N_("Return to the previous document in history")) },
	{ "history-move-forward", ACT_MAIN_HISTORY_MOVE_FORWARD, DACT(N_("Go forward in history")) },
	{ "jump-to-link", ACT_MAIN_JUMP_TO_LINK, DACT(N_("Jump to link")) },
	{ "keybinding-manager", ACT_MAIN_KEYBINDING_MANAGER, DACT(N_("Open keybinding manager")) },
	{ "kill-backgrounded-connections", ACT_MAIN_KILL_BACKGROUNDED_CONNECTIONS, DACT(N_("Kill all backgrounded connections")) },
	{ "link-download", ACT_MAIN_LINK_DOWNLOAD, DACT(N_("Download the current link")) },
	{ "link-download-image", ACT_MAIN_LINK_DOWNLOAD_IMAGE, DACT(N_("Download the current image")) },
	{ "link-download-resume", ACT_MAIN_LINK_DOWNLOAD_RESUME, DACT(N_("Attempt to resume download of the current link")) },
	{ "link-external-command", ACT_MAIN_LINK_EXTERNAL_COMMAND, DACT(N_("Pass URI of current link to external command")) },
	{ "link-follow", ACT_MAIN_LINK_FOLLOW, DACT(N_("Follow the current link")) },
	{ "link-follow-reload", ACT_MAIN_LINK_FOLLOW_RELOAD, DACT(N_("Follow the current link, forcing reload of the target")) },
	{ "link-menu", ACT_MAIN_LINK_MENU, DACT(N_("Open the link context menu")) },
#ifdef CONFIG_LUA
	{ "lua-console", ACT_MAIN_LUA_CONSOLE, DACT(N_("Open a Lua console")) },
#else
	{ "lua-console", ACT_MAIN_LUA_CONSOLE, DACT(N_("Open a Lua console (DISABLED)")) },
#endif
	{ "mark-goto", ACT_MAIN_MARK_GOTO, DACT(N_("Go at a specified mark")) },
	{ "mark-set", ACT_MAIN_MARK_SET, DACT(N_("Set a mark")) },
	{ "menu", ACT_MAIN_MENU, DACT(N_("Activate the menu")) },
	{ "move-cursor-down", ACT_MAIN_MOVE_CURSOR_DOWN, DACT(N_("Move cursor down")) },
	{ "move-cursor-left", ACT_MAIN_MOVE_CURSOR_LEFT, DACT(N_("Move cursor left")) },
	{ "move-cursor-right", ACT_MAIN_MOVE_CURSOR_RIGHT, DACT(N_("Move cursor right")) },
	{ "move-cursor-up", ACT_MAIN_MOVE_CURSOR_UP, DACT(N_("Move cursor up")) },
	{ "move-document-end", ACT_MAIN_MOVE_DOCUMENT_END, DACT(N_("Move to the end of the document")) },
	{ "move-document-start", ACT_MAIN_MOVE_DOCUMENT_START, DACT(N_("Move to the start of the document")) },
	{ "move-link-down", ACT_MAIN_MOVE_LINK_DOWN, DACT(N_("Move one link down")) },
	{ "move-link-left", ACT_MAIN_MOVE_LINK_LEFT, DACT(N_("Move one link left")) },
	{ "move-link-next", ACT_MAIN_MOVE_LINK_NEXT, DACT(N_("Move to the next link")) },
	{ "move-link-prev", ACT_MAIN_MOVE_LINK_PREV, DACT(N_("Move to the previous link")) },
	{ "move-link-right", ACT_MAIN_MOVE_LINK_RIGHT, DACT(N_("Move one link right")) },
	{ "move-link-up", ACT_MAIN_MOVE_LINK_UP, DACT(N_("Move one link up")) },
	{ "move-page-down", ACT_MAIN_MOVE_PAGE_DOWN, DACT(N_("Move downwards by a page")) },
	{ "move-page-up", ACT_MAIN_MOVE_PAGE_UP, DACT(N_("Move upwards by a page")) },
	{ "open-link-in-new-tab", ACT_MAIN_OPEN_LINK_IN_NEW_TAB, DACT(N_("Open the current link in a new tab")) },
	{ "open-link-in-new-tab-in-background", ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open the current link in a new tab in the background")) },
	{ "open-link-in-new-window", ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW, DACT(N_("Open the current link in a new window")) },
	{ "open-new-tab", ACT_MAIN_OPEN_NEW_TAB, DACT(N_("Open a new tab")) },
	{ "open-new-tab-in-background", ACT_MAIN_OPEN_NEW_TAB_IN_BACKGROUND, DACT(N_("Open a new tab in the background")) },
	{ "open-new-window", ACT_MAIN_OPEN_NEW_WINDOW, DACT(N_("Open a new window")) },
	{ "open-os-shell", ACT_MAIN_OPEN_OS_SHELL, DACT(N_("Open an OS shell")) },
	{ "options-manager", ACT_MAIN_OPTIONS_MANAGER, DACT(N_("Open options manager")) },
	{ "quit", ACT_MAIN_QUIT, DACT(N_("Open a quit confirmation dialog box")) },
	{ "really-quit", ACT_MAIN_REALLY_QUIT, DACT(N_("Quit without confirmation")) },
	{ "redraw", ACT_MAIN_REDRAW, DACT(N_("Redraw the terminal")) },
	{ "reload", ACT_MAIN_RELOAD, DACT(N_("Reload the current page")) },
	{ "rerender", ACT_MAIN_RERENDER, DACT(N_("Re-render the current page")) },
	{ "reset-form", ACT_MAIN_RESET_FORM, DACT(N_("Reset form items to their initial values")) },
	{ "resource-info", ACT_MAIN_RESOURCE_INFO, DACT(N_("Show information about the currently used resources")) },
	{ "save-as", ACT_MAIN_SAVE_AS, DACT(N_("Save the current document in source form")) },
	{ "save-formatted", ACT_MAIN_SAVE_FORMATTED, DACT(N_("Save the current document in formatted form")) },
	{ "save-options", ACT_MAIN_SAVE_OPTIONS, DACT(N_("Save options")), },
	{ "save-url-as", ACT_MAIN_SAVE_URL_AS, DACT(N_("Save URL as")) },
	{ "scroll-down", ACT_MAIN_SCROLL_DOWN, DACT(N_("Scroll down")) },
	{ "scroll-left", ACT_MAIN_SCROLL_LEFT, DACT(N_("Scroll left")) },
	{ "scroll-right", ACT_MAIN_SCROLL_RIGHT, DACT(N_("Scroll right")) },
	{ "scroll-up", ACT_MAIN_SCROLL_UP, DACT(N_("Scroll up")) },
	{ "search", ACT_MAIN_SEARCH, DACT(N_("Search for a text pattern")) },
	{ "search-back", ACT_MAIN_SEARCH_BACK, DACT(N_("Search backwards for a text pattern")) },
	{ "search-typeahead", ACT_MAIN_SEARCH_TYPEAHEAD, DACT(N_("Search link text by typing ahead")) },
	{ "search-typeahead-link", ACT_MAIN_SEARCH_TYPEAHEAD_LINK, DACT(N_("Search link text by typing ahead")) },
	{ "search-typeahead-text", ACT_MAIN_SEARCH_TYPEAHEAD_TEXT, DACT(N_("Search document text by typing ahead")) },
	{ "search-typeahead-text-back", ACT_MAIN_SEARCH_TYPEAHEAD_TEXT_BACK, DACT(N_("Search document text backwards by typing ahead")) },
	{ "show-term-options", ACT_MAIN_SHOW_TERM_OPTIONS, DACT(N_("Show terminal options dialog")) },
	{ "submit-form", ACT_MAIN_SUBMIT_FORM, DACT(N_("Submit form")) },
	{ "submit-form-reload", ACT_MAIN_SUBMIT_FORM_RELOAD, DACT(N_("Submit form and reload")) },
	{ "tab-close", ACT_MAIN_TAB_CLOSE, DACT(N_("Close tab")) },
	{ "tab-close-all-but-current", ACT_MAIN_TAB_CLOSE_ALL_BUT_CURRENT, DACT(N_("Close all tabs but the current one")) },
	{ "tab-external-command", ACT_MAIN_TAB_EXTERNAL_COMMAND, DACT(N_("Pass URI of current tab to external command")) },
	{ "tab-menu", ACT_MAIN_TAB_MENU, DACT(N_("Open the tab menu")) },
	{ "tab-move-left", ACT_MAIN_TAB_MOVE_LEFT, DACT(N_("Move the current tab to the left")) },
	{ "tab-move-right", ACT_MAIN_TAB_MOVE_RIGHT, DACT(N_("Move the current tab to the right")) },
	{ "tab-next", ACT_MAIN_TAB_NEXT, DACT(N_("Next tab")) },
	{ "tab-prev", ACT_MAIN_TAB_PREV, DACT(N_("Previous tab")) },
	{ "terminal-resize", ACT_MAIN_TERMINAL_RESIZE, DACT(N_("Open the terminal resize dialog")) },
	{ "toggle-css", ACT_MAIN_TOGGLE_CSS, DACT(N_("Toggle rendering of page using CSS")) },
	{ "toggle-display-images", ACT_MAIN_TOGGLE_DISPLAY_IMAGES, DACT(N_("Toggle displaying of links to images")) },
	{ "toggle-display-tables", ACT_MAIN_TOGGLE_DISPLAY_TABLES, DACT(N_("Toggle rendering of tables")) },
	{ "toggle-document-colors", ACT_MAIN_TOGGLE_DOCUMENT_COLORS, DACT(N_("Toggle usage of document specific colors")) },
	{ "toggle-html-plain", ACT_MAIN_TOGGLE_HTML_PLAIN, DACT(N_("Toggle rendering page as HTML / plain text")) },
	{ "toggle-numbered-links", ACT_MAIN_TOGGLE_NUMBERED_LINKS, DACT(N_("Toggle displaying of links numbers")) },
	{ "toggle-plain-compress-empty-lines", ACT_MAIN_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES, DACT(N_("Toggle plain renderer compression of empty lines")) },
	{ "toggle-wrap-text", ACT_MAIN_TOGGLE_WRAP_TEXT, DACT(N_("Toggle wrapping of text")) },
	{ "view-image", ACT_MAIN_VIEW_IMAGE, DACT(N_("View the current image")) },

	{ NULL, 0, NULL }
};

static struct strtonum edit_action_table[EDIT_ACTIONS + 1] = {
	{ "none", ACT_EDIT_NONE, DACT(N_("Do nothing")) },
	{ " *scripting-function*", ACT_EDIT_SCRIPTING_FUNCTION, NULL }, /* internal use only */
	{ "auto-complete", ACT_EDIT_AUTO_COMPLETE, DACT(N_("Attempt to auto-complete the input")) },
	{ "auto-complete-unambiguous", ACT_EDIT_AUTO_COMPLETE_UNAMBIGUOUS, DACT(N_("Attempt to unambiguously auto-complete the input")) },
	{ "backspace", ACT_EDIT_BACKSPACE, DACT(N_("Delete character in front of the cursor")) },
	{ "beginning-of-buffer", ACT_EDIT_BEGINNING_OF_BUFFER, DACT(N_("Go to the first line of the buffer")) },
	{ "cancel", ACT_EDIT_CANCEL, DACT(N_("Cancel current state")) },
	{ "copy-clipboard", ACT_EDIT_COPY_CLIPBOARD, DACT(N_("Copy text to clipboard")) },
	{ "cut-clipboard", ACT_EDIT_CUT_CLIPBOARD, DACT(N_("Delete text from clipboard")) },
	{ "delete", ACT_EDIT_DELETE, DACT(N_("Delete character under cursor")) },
	{ "down", ACT_EDIT_DOWN, DACT(N_("Move cursor downwards")) },
	{ "end", ACT_EDIT_END, DACT(N_("Go to the end of the page/line")) },
	{ "end-of-buffer", ACT_EDIT_END_OF_BUFFER, DACT(N_("Go to the last line of the buffer")) },
	{ "enter", ACT_EDIT_ENTER, DACT(N_("Follow the current link")) },
	{ "home", ACT_EDIT_HOME, DACT(N_("Go to the start of the page/line")) },
	{ "kill-to-bol", ACT_EDIT_KILL_TO_BOL, DACT(N_("Delete to beginning of line")) },
	{ "kill-to-eol", ACT_EDIT_KILL_TO_EOL, DACT(N_("Delete to end of line")) },
	{ "left", ACT_EDIT_LEFT, DACT(N_("Move the cursor left")) },
	{ "next-item", ACT_EDIT_NEXT_ITEM, DACT(N_("Move to the next item")) },
	{ "open-external", ACT_EDIT_OPEN_EXTERNAL, DACT(N_("Open in external editor")) },
	{ "paste-clipboard", ACT_EDIT_PASTE_CLIPBOARD, DACT(N_("Paste text from the clipboard")) },
	{ "previous-item", ACT_EDIT_PREVIOUS_ITEM, DACT(N_("Move to the previous item")) },
	{ "redraw", ACT_EDIT_REDRAW, DACT(N_("Redraw the terminal")) },
	{ "right", ACT_EDIT_RIGHT, DACT(N_("Move the cursor right")) },
	{ "search-toggle-regex", ACT_EDIT_SEARCH_TOGGLE_REGEX, DACT(N_("Toggle regex matching (type-ahead searching)")) },
	{ "up", ACT_EDIT_UP, DACT(N_("Move cursor upwards")) },

	{ NULL, 0, NULL }
};

static struct strtonum menu_action_table[MENU_ACTIONS + 1] = {
	{ "none", ACT_MENU_NONE, DACT(N_("Do nothing")) },
	{ " *scripting-function*", ACT_MENU_SCRIPTING_FUNCTION, NULL }, /* internal use only */
	{ "cancel", ACT_MENU_CANCEL, DACT(N_("Cancel current state")) },
	{ "delete", ACT_MENU_DELETE, DACT(N_("Delete character under cursor")) },
	{ "down", ACT_MENU_DOWN, DACT(N_("Move cursor downwards")) },
	{ "end", ACT_MENU_END, DACT(N_("Go to the end of the page/line")) },
	{ "enter", ACT_MENU_ENTER, DACT(N_("Follow the current link")) },
	{ "expand", ACT_MENU_EXPAND, DACT(N_("Expand item")) },
	{ "home", ACT_MENU_HOME, DACT(N_("Go to the start of the page/line")) },
	{ "left", ACT_MENU_LEFT, DACT(N_("Move the cursor left")) },
	{ "mark-item", ACT_MENU_MARK_ITEM, DACT(N_("Mark item")) },
	{ "next-item", ACT_MENU_NEXT_ITEM, DACT(N_("Move to the next item")) },
	{ "page-down", ACT_MENU_PAGE_DOWN, DACT(N_("Move downwards by a page")) },
	{ "page-up", ACT_MENU_PAGE_UP, DACT(N_("Move upwards by a page")) },
	{ "previous-item", ACT_MENU_PREVIOUS_ITEM, DACT(N_("Move to the previous item")) },
	{ "redraw", ACT_MENU_REDRAW, DACT(N_("Redraw the terminal")) },
	{ "right", ACT_MENU_RIGHT, DACT(N_("Move the cursor right")) },
	{ "search", ACT_MENU_SEARCH, DACT(N_("Search for a text pattern")) },
	{ "select", ACT_MENU_SELECT, DACT(N_("Select current highlighted item")) },
	{ "unexpand", ACT_MENU_UNEXPAND, DACT(N_("Collapse item")) },
	{ "up", ACT_MENU_UP, DACT(N_("Move cursor upwards")) },

	{ NULL, 0, NULL }
};

static struct strtonum *action_table[KEYMAP_MAX] = {
	main_action_table,
	edit_action_table,
	menu_action_table,
};

#undef DACT

int
read_action(enum keymap keymap, unsigned char *action)
{
	assert(keymap >= 0 && keymap < KEYMAP_MAX);
	return strtonum(action_table[keymap], action);
}

unsigned char *
write_action(enum keymap keymap, int action)
{
	assert(keymap >= 0 && keymap < KEYMAP_MAX);
	return numtostr(action_table[keymap], action);
}


void
init_keymaps(void)
{
	enum keymap i;

	for (i = 0; i < KEYMAP_MAX; i++)
		init_list(keymaps[i]);

	init_keybinding_listboxes(keymap_table, action_table);
	add_default_keybindings();
}

void
free_keymaps(void)
{
	enum keymap i;

	done_keybinding_listboxes();

	for (i = 0; i < KEYMAP_MAX; i++)
		free_list(keymaps[i]);
}


/*
 * Bind to Lua function.
 */

#ifdef CONFIG_SCRIPTING
unsigned char *
bind_scripting_func(unsigned char *ckmap, unsigned char *ckey, int func_ref)
{
	unsigned char *err = NULL;
	long key, meta;
	int action;
	int kmap = read_keymap(ckmap);

	if (kmap < 0)
		err = gettext("Unrecognised keymap");
	else if (parse_keystroke(ckey, &key, &meta) < 0)
		err = gettext("Error parsing keystroke");
	else if ((action = read_action(kmap, " *scripting-function*")) < 0)
		err = gettext("Unrecognised action (internal error)");
	else
		add_keybinding(kmap, action, key, meta, func_ref);

	return err;
}
#endif


/*
 * Default keybindings.
 */

struct default_kb {
	long key;
	long meta;
	int action;
};

static struct default_kb default_main_keymap[] = {
	{ ' ',		 0,		ACT_MAIN_MOVE_PAGE_DOWN },
	{ '#',		 0,		ACT_MAIN_SEARCH_TYPEAHEAD },
	{ '%',		 0,		ACT_MAIN_TOGGLE_DOCUMENT_COLORS },
	{ '*',		 0,		ACT_MAIN_TOGGLE_DISPLAY_IMAGES },
	{ ',',		 0,		ACT_MAIN_LUA_CONSOLE },
	{ '.',		 0,		ACT_MAIN_TOGGLE_NUMBERED_LINKS },
	{ '/',		 0,		ACT_MAIN_SEARCH },
	{ ':',		 0,		ACT_MAIN_EXMODE },
	{ '<',		 0,		ACT_MAIN_TAB_PREV },
	{ '<',		 KBD_ALT,	ACT_MAIN_TAB_MOVE_LEFT },
	{ '=',		 0,		ACT_MAIN_DOCUMENT_INFO },
	{ '>',		 0,		ACT_MAIN_TAB_NEXT },
	{ '>',		 KBD_ALT,	ACT_MAIN_TAB_MOVE_RIGHT },
	{ '?',		 0,		ACT_MAIN_SEARCH_BACK },
	{ 'A',		 0,		ACT_MAIN_ADD_BOOKMARK_LINK },
	{ 'A',		 KBD_CTRL,	ACT_MAIN_MOVE_DOCUMENT_START },
	{ 'B',		 KBD_CTRL,	ACT_MAIN_MOVE_PAGE_UP },
	{ 'C',		 0,		ACT_MAIN_CACHE_MANAGER },
	{ 'D',		 0,		ACT_MAIN_DOWNLOAD_MANAGER },
	{ 'E',		 0,		ACT_MAIN_GOTO_URL_CURRENT_LINK },
	{ 'E',		 KBD_CTRL,	ACT_MAIN_MOVE_DOCUMENT_END },
	{ 'F',		 0,		ACT_MAIN_FORMHIST_MANAGER },
	{ 'F',		 KBD_CTRL,	ACT_MAIN_MOVE_PAGE_DOWN },
	{ 'G',		 0,		ACT_MAIN_GOTO_URL_CURRENT },
	{ 'H',		 0,		ACT_MAIN_GOTO_URL_HOME },
	{ 'K',		 0,		ACT_MAIN_COOKIE_MANAGER },
	{ 'K',		 KBD_CTRL,	ACT_MAIN_COOKIES_LOAD },
	{ 'L',		 0,		ACT_MAIN_LINK_MENU },
	{ 'L',		 KBD_CTRL,	ACT_MAIN_REDRAW },
	{ 'N',		 0,		ACT_MAIN_FIND_NEXT_BACK },
	{ 'N',		 KBD_CTRL,	ACT_MAIN_SCROLL_DOWN },
	{ 'P',		 KBD_CTRL,	ACT_MAIN_SCROLL_UP },
	{ 'Q',		 0,		ACT_MAIN_REALLY_QUIT },
	{ 'R',		 KBD_CTRL,	ACT_MAIN_RELOAD },
	{ 'T',		 0,		ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND },
	{ 'W',		 0,		ACT_MAIN_TOGGLE_WRAP_TEXT },
	{ '[',		 0,		ACT_MAIN_SCROLL_LEFT },
	{ '\'',		 0,		ACT_MAIN_MARK_GOTO },
	{ '\\',		 0,		ACT_MAIN_TOGGLE_HTML_PLAIN },
	{ ']',		 0,		ACT_MAIN_SCROLL_RIGHT },
	{ '_',		 0,		ACT_MAIN_TOGGLE_CSS },
	{ 'a',		 0,		ACT_MAIN_ADD_BOOKMARK },
	{ 'b',		 0,		ACT_MAIN_MOVE_PAGE_UP },
	{ 'c',		 0,		ACT_MAIN_TAB_CLOSE },
	{ 'd',		 0,		ACT_MAIN_LINK_DOWNLOAD },
	{ 'e',		 0,		ACT_MAIN_TAB_MENU },
	{ 'f',		 0,		ACT_MAIN_FRAME_MAXIMIZE },
	{ 'g',		 0,		ACT_MAIN_GOTO_URL },
	{ 'h',		 0,		ACT_MAIN_HISTORY_MANAGER },
	{ 'k',		 0,		ACT_MAIN_KEYBINDING_MANAGER },
	{ 'l',		 0,		ACT_MAIN_JUMP_TO_LINK },
	{ 'm',		 0,		ACT_MAIN_MARK_SET },
	{ 'n',		 0,		ACT_MAIN_FIND_NEXT },
	{ 'o',		 0,		ACT_MAIN_OPTIONS_MANAGER },
	{ 'q',		 0,		ACT_MAIN_QUIT },
	{ 'r',		 0,		ACT_MAIN_LINK_DOWNLOAD_RESUME },
	{ 's',		 0,		ACT_MAIN_BOOKMARK_MANAGER },
	{ 't',		 0,		ACT_MAIN_OPEN_NEW_TAB },
	{ 'u',		 0,		ACT_MAIN_HISTORY_MOVE_FORWARD },
	{ 'v',		 0,		ACT_MAIN_VIEW_IMAGE },
	{ 'w',		 0,		ACT_MAIN_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES },
	{ 'x',		 0,		ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ 'z',		 0,		ACT_MAIN_ABORT_CONNECTION },
	{ '{',		 0,		ACT_MAIN_SCROLL_LEFT },
	{ '|',		 0,		ACT_MAIN_HEADER_INFO },
	{ '}',		 0,		ACT_MAIN_SCROLL_RIGHT },
	{ KBD_DEL,	 0,		ACT_MAIN_SCROLL_DOWN },
	{ KBD_DOWN,	 0,		ACT_MAIN_MOVE_LINK_NEXT },
	{ KBD_END,	 0,		ACT_MAIN_MOVE_DOCUMENT_END },
	{ KBD_ENTER,	 0,		ACT_MAIN_LINK_FOLLOW },
	{ KBD_ENTER,	 KBD_CTRL,	ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ KBD_ESC,	 0,		ACT_MAIN_MENU },
	{ KBD_F10,	 0,		ACT_MAIN_FILE_MENU },
	{ KBD_F9,	 0,		ACT_MAIN_MENU },
	{ KBD_HOME,	 0,		ACT_MAIN_MOVE_DOCUMENT_START },
	{ KBD_INS,	 0,		ACT_MAIN_SCROLL_UP },
	{ KBD_INS,	 KBD_CTRL,	ACT_MAIN_COPY_CLIPBOARD },
	{ KBD_LEFT,	 0,		ACT_MAIN_HISTORY_MOVE_BACK },
	{ KBD_PAGE_DOWN, 0,		ACT_MAIN_MOVE_PAGE_DOWN },
	{ KBD_PAGE_UP,	 0,		ACT_MAIN_MOVE_PAGE_UP },
	{ KBD_RIGHT,	 0,		ACT_MAIN_LINK_FOLLOW },
	{ KBD_RIGHT,	 KBD_CTRL,	ACT_MAIN_LINK_FOLLOW_RELOAD },
	{ KBD_TAB,	 0,		ACT_MAIN_FRAME_NEXT },
	{ KBD_TAB,	 KBD_ALT,	ACT_MAIN_FRAME_PREV },
	{ KBD_UP,	 0,		ACT_MAIN_MOVE_LINK_PREV },
	{ 0, 0, 0 }
};

static struct default_kb default_edit_keymap[] = {
	{ '<',		 KBD_ALT,	ACT_EDIT_BEGINNING_OF_BUFFER },
	{ '>',		 KBD_ALT,	ACT_EDIT_END_OF_BUFFER },
	{ 'A',		 KBD_CTRL,	ACT_EDIT_HOME },
	{ 'D',		 KBD_CTRL,	ACT_EDIT_DELETE },
	{ 'E',		 KBD_CTRL,	ACT_EDIT_END },
	{ 'H',		 KBD_CTRL,	ACT_EDIT_BACKSPACE },
	{ 'K',		 KBD_CTRL,	ACT_EDIT_KILL_TO_EOL },
	{ 'L',		 KBD_CTRL,	ACT_EDIT_REDRAW },
	{ 'r',		 KBD_ALT,	ACT_EDIT_SEARCH_TOGGLE_REGEX },
	{ 'R',		 KBD_CTRL,	ACT_EDIT_AUTO_COMPLETE_UNAMBIGUOUS },
	{ 'T',		 KBD_CTRL,	ACT_EDIT_OPEN_EXTERNAL },
	{ 'U',		 KBD_CTRL,	ACT_EDIT_KILL_TO_BOL },
	{ 'V',		 KBD_CTRL,	ACT_EDIT_PASTE_CLIPBOARD },
	{ 'W',		 KBD_CTRL,	ACT_EDIT_AUTO_COMPLETE },
	{ 'X',		 KBD_CTRL,	ACT_EDIT_CUT_CLIPBOARD },
	{ KBD_BS,	 0,		ACT_EDIT_BACKSPACE },
	{ KBD_DEL,	 0,		ACT_EDIT_DELETE },
	{ KBD_DOWN,	 0,		ACT_EDIT_DOWN },
	{ KBD_END,	 0,		ACT_EDIT_END },
	{ KBD_ENTER,	 0,		ACT_EDIT_ENTER },
	{ KBD_ESC,	 0,		ACT_EDIT_CANCEL },
	{ KBD_F4,	 0,		ACT_EDIT_OPEN_EXTERNAL },
	{ KBD_HOME,	 0,		ACT_EDIT_HOME },
	{ KBD_INS,	 KBD_CTRL,	ACT_EDIT_COPY_CLIPBOARD },
	{ KBD_LEFT,	 0,		ACT_EDIT_LEFT },
	{ KBD_RIGHT,	 0,		ACT_EDIT_RIGHT },
	{ KBD_TAB,	 0,		ACT_EDIT_NEXT_ITEM },
	{ KBD_TAB,	 KBD_ALT,	ACT_EDIT_PREVIOUS_ITEM },
	{ KBD_UP,	 0,		ACT_EDIT_UP },
	{ 0, 0, 0 }
};

static struct default_kb default_menu_keymap[] = {
	{ ' ',		 0,		ACT_MENU_SELECT },
	{ '*',		 0,		ACT_MENU_MARK_ITEM },
	{ '+',		 0,		ACT_MENU_EXPAND },
	{ '-',		 0,		ACT_MENU_UNEXPAND },
	{ '/',		 0,		ACT_MENU_SEARCH },
	{ '=',		 0,		ACT_MENU_EXPAND },
	{ 'A',		 KBD_CTRL,	ACT_MENU_HOME },
	{ 'B',		 KBD_CTRL,	ACT_MENU_PAGE_UP },
	{ 'E',		 KBD_CTRL,	ACT_MENU_END },
	{ 'F',		 KBD_CTRL,	ACT_MENU_PAGE_DOWN },
	{ 'L',		 KBD_CTRL,	ACT_MENU_REDRAW },
	{ 'N',		 KBD_CTRL,	ACT_MENU_DOWN },
	{ 'P',		 KBD_CTRL,	ACT_MENU_UP },
	{ 'V',		 KBD_ALT,	ACT_MENU_PAGE_UP },
	{ 'V',		 KBD_CTRL,	ACT_MENU_PAGE_DOWN },
	{ '[',		 0,		ACT_MENU_EXPAND },
	{ ']',		 0,		ACT_MENU_UNEXPAND },
	{ '_',		 0,		ACT_MENU_UNEXPAND },
	{ KBD_DEL,	 0,		ACT_MENU_DELETE },
	{ KBD_DOWN,	 0,		ACT_MENU_DOWN },
	{ KBD_END,	 0,		ACT_MENU_END },
	{ KBD_ENTER,	 0,		ACT_MENU_ENTER },
	{ KBD_ESC,	 0,		ACT_MENU_CANCEL },
	{ KBD_HOME,	 0,		ACT_MENU_HOME },
	{ KBD_INS,	 0,		ACT_MENU_MARK_ITEM },
	{ KBD_LEFT,	 0,		ACT_MENU_LEFT },
	{ KBD_PAGE_DOWN, 0,		ACT_MENU_PAGE_DOWN },
	{ KBD_PAGE_UP,	 0,		ACT_MENU_PAGE_UP },
	{ KBD_RIGHT,	 0,		ACT_MENU_RIGHT },
	{ KBD_TAB,	 0,		ACT_MENU_NEXT_ITEM },
	{ KBD_TAB,	 KBD_ALT,	ACT_MENU_PREVIOUS_ITEM },
	{ KBD_UP,	 0,		ACT_MENU_UP },
	{ 0, 0, 0}
};

static struct default_kb *default_keybindings[] = {
	default_main_keymap,
	default_edit_keymap,
	default_menu_keymap,
};

static int
keybinding_is_default(struct keybinding *kb)
{
	struct default_kb keybinding = { kb->key, kb->meta, kb->action };
	struct default_kb *pos;

	for (pos = default_keybindings[kb->keymap]; pos->key; pos++)
		if (!memcmp(&keybinding, pos, sizeof(keybinding)))
			return 1;

	return 0;
}

static void
add_default_keybindings(void)
{
	/* Maybe we shouldn't delete old keybindings. But on the other side, we
	 * can't trust clueless users what they'll push into sources modifying
	 * defaults, can we? ;)) */
	enum keymap keymap;

	for (keymap = 0; keymap < KEYMAP_MAX; keymap++) {
		struct default_kb *kb;

		for (kb = default_keybindings[keymap]; kb->key; kb++) {
			struct keybinding *keybinding;

			keybinding = add_keybinding(keymap, kb->action, kb->key,
						    kb->meta, EVENT_NONE);
			keybinding->flags |= KBDB_DEFAULT;
		}
	}
}


/*
 * Config file tools.
 */

static struct strtonum main_action_aliases[] = {
	{ "back", ACT_MAIN_HISTORY_MOVE_BACK, "history-move-back" },
	{ "down", ACT_MAIN_MOVE_LINK_NEXT, "move-link-next" },
	{ "download", ACT_MAIN_LINK_DOWNLOAD, "link-download" },
	{ "download-image", ACT_MAIN_LINK_DOWNLOAD_IMAGE, "link-download-image" },
	{ "end", ACT_MAIN_MOVE_DOCUMENT_END, "move-document-end" },
	{ "enter", ACT_MAIN_LINK_FOLLOW, "link-follow" },
	{ "enter-reload", ACT_MAIN_LINK_FOLLOW_RELOAD, "link-follow-reload" },
	{ "home", ACT_MAIN_MOVE_DOCUMENT_START, "move-document-start" },
	{ "next-frame", ACT_MAIN_FRAME_NEXT, "frame-next" },
	{ "page-down", ACT_MAIN_MOVE_PAGE_DOWN, "move-page-down" },
	{ "page-up", ACT_MAIN_MOVE_PAGE_UP, "move-page-up" },
	{ "previous-frame", ACT_MAIN_FRAME_PREV, "frame-prev" },
	{ "resume-download", ACT_MAIN_LINK_DOWNLOAD_RESUME, "link-download-resume" },
	{ "unback", ACT_MAIN_HISTORY_MOVE_FORWARD, "history-move-forward" },
	{ "up",	ACT_MAIN_MOVE_LINK_PREV, "move-link-prev" },
	{ "zoom-frame", ACT_MAIN_FRAME_MAXIMIZE, "frame-maximize" },

	{ NULL, 0, NULL }
};

static struct strtonum edit_action_aliases[] = {
	{ "edit", ACT_EDIT_OPEN_EXTERNAL, "open-external" },

	{ NULL, 0, NULL }
};

static struct strtonum *action_aliases[KEYMAP_MAX] = {
	main_action_aliases,
	edit_action_aliases,
	NULL,
};

static int
get_aliased_action(enum keymap keymap, unsigned char *action)
{
	int alias = -1;

	assert(keymap >= 0 && keymap < KEYMAP_MAX);

	if (action_aliases[keymap])
		alias = strtonum(action_aliases[keymap], action);

	return alias < 0 ? read_action(keymap, action) : alias;
}

/* Return 0 when ok, something strange otherwise. */
int
bind_do(unsigned char *keymap, unsigned char *keystroke, unsigned char *action)
{
	int keymap_, action_;
	long key_, meta_;

	keymap_ = read_keymap(keymap);
	if (keymap_ < 0) return 1;

	if (parse_keystroke(keystroke, &key_, &meta_) < 0) return 2;

	action_ = get_aliased_action(keymap_, action);
	if (action_ < 0) return 77 / 9 - 5;

	add_keybinding(keymap_, action_, key_, meta_, EVENT_NONE);
	return 0;
}

unsigned char *
bind_act(unsigned char *keymap, unsigned char *keystroke)
{
	int keymap_;
	long key_, meta_;
	unsigned char *action;
	struct keybinding *kb;

	keymap_ = read_keymap(keymap);
	if (keymap_ < 0)
		return NULL;

	if (parse_keystroke(keystroke, &key_, &meta_) < 0)
		return NULL;

	kb = kbd_ev_lookup(keymap_, key_, meta_, NULL);
	if (!kb) return NULL;

	action = write_action(keymap_, kb->action);
	if (!action)
		return NULL;

	kb->flags |= KBDB_WATERMARK;
	return straconcat("\"", action, "\"", NULL);
}

static void
single_bind_config_string(struct string *file, enum keymap keymap,
			  struct keybinding *keybinding)
{
	unsigned char *keymap_str = write_keymap(keymap);
	unsigned char *action_str = write_action(keymap, keybinding->action);

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
	make_keystroke(file, keybinding->key, keybinding->meta, 1);
	add_to_string(file, "\" = \"");
	add_to_string(file, action_str);
	add_char_to_string(file, '\"');
	add_char_to_string(file, '\n');
}

void
bind_config_string(struct string *file)
{
	enum keymap keymap;

	for (keymap = 0; keymap < KEYMAP_MAX; keymap++) {
		struct keybinding *keybinding;

		foreach (keybinding, keymaps[keymap]) {
			/* Don't save default keybindings that has not been
			 * deleted (rebound to none action) (Bug 337). */
			/* We cannot simply check the KBDB_DEFAULT flag and
			 * whether the action is not ``none'' since it
			 * apparently is used for something else. */
			if (keybinding_is_default(keybinding))
				continue;

			single_bind_config_string(file, keymap, keybinding);
		}
	}
}
