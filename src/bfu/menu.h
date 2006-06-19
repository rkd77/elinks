#ifndef EL__BFU_MENU_H
#define EL__BFU_MENU_H

#include "config/kbdbind.h"
#include "util/box.h"

struct terminal;
struct window;

typedef void (*menu_func_T)(struct terminal *, void *, void *);

/* Which fields to free when zapping a list item - bitwise. */
enum menu_item_flags {
	NO_FLAG = 0,

	FREE_LIST = 1,		/* Free the 'list' of menu items */

	FREE_TEXT = 2,		/* Free the (main) text */
	FREE_RTEXT = 4,		/* Free the right aligned text */
	FREE_DATA = 8,		/* Free the private data */

	MENU_FULLNAME = 16,	/* Catenate base string to <select> item text */
	SUBMENU = 32,		/* Item opens submenu, so show '>>' as rtext */
	NO_INTL = 64,		/* Don't translate the text */
	NO_SELECT = 128,	/* Mark item as unselectable */
	RIGHT_INTL = 256,	/* Force translation of the right text */
};

#define FREE_ANY (FREE_LIST|FREE_TEXT|FREE_RTEXT|FREE_DATA)

/*
 * Selectable menu item.
 */
#define mi_is_selectable(mi) (!((mi)->flags & NO_SELECT))

/*
 * Menu item has left text.
 */
#define mi_has_left_text(mi) ((mi)->text && *(mi)->text)

/*
 * Menu item has right text.
 */
#define mi_has_right_text(mi) ((mi)->rtext && *(mi)->rtext)

/*
 * Horizontal bar
 */
#define mi_is_horizontal_bar(mi) (!mi_is_selectable(mi) && (mi)->text && !(mi)->text[0])

/*
 * Submenu item
 */
#define mi_is_submenu(mi) ((mi)->flags & SUBMENU)

/*
 * Texts should be translated or not.
 */
#define mi_text_translate(mi) (!((mi)->flags & NO_INTL))
#define mi_rtext_translate(mi) ((mi)->flags & RIGHT_INTL)

/*
 * End of menu items list
 */
#define mi_is_end_of_menu(mi) (!(mi)->text)

#define foreach_menu_item(iterator, items) \
	for (iterator = (items); !mi_is_end_of_menu(iterator); (iterator)++)

enum hotkey_state {
	HKS_SHOW = 0,
	HKS_IGNORE,
	HKS_CACHED,
};

/* XXX: keep order of fields, there's some hard initializations for it. --Zas
 */
struct menu_item {
	unsigned char *text;		/* The item label */

	/* The following three members are tightly coupled:
	 *
	 * - If @action is not MAIN_ACT_NONE the associated keybinding will be
	 *   shown as the guiding right text and do_action() will be called
	 *   when the item is selected rendering both @rtext and @func useless.
	 *
	 * - A few places however there is no associated keybinding and no
	 *   ``default'' handler defined in which case @rtext (if non NULL)
	 *   will be drawn and @func will be called when selecting the item. */
	unsigned char *rtext;		/* Right aligned guiding text */
	enum main_action action_id;	/* Default item handlers */
	menu_func_T func;		/* Called when selecting the item */

	void *data;			/* Private data passed to handler */
	enum menu_item_flags flags;	/* What to free() and display */

	/* If true, don't try to translate text/rtext inside of the menu
	 * routines. */
	enum hotkey_state hotkey_state;	/* The state of the hotkey caching */
	int hotkey_pos;			/* The offset of the hotkey in @text */
};

#define INIT_MENU_ITEM(text, rtext, action_id, func, data, flags)	\
{									\
	(unsigned char *) (text),					\
	(unsigned char *) (rtext),					\
	(action_id),							\
	(func),								\
	(void *) (data),						\
	(flags),							\
	HKS_SHOW,							\
	0								\
}

#define INIT_MENU_ACTION(text, action_id)				\
	INIT_MENU_ITEM(text, NULL, action_id, NULL, NULL, 0)

#define NULL_MENU_ITEM							\
	INIT_MENU_ITEM(NULL, NULL, ACT_MAIN_NONE, NULL, NULL, 0)

#define BAR_MENU_ITEM							\
	INIT_MENU_ITEM("", NULL, ACT_MAIN_NONE, NULL, NULL, NO_SELECT)

#define SET_MENU_ITEM(e_, text_, rtext_, action_id_, func_, data_,	\
		      flags_, hotkey_state_, hotkey_pos_)		\
do {									\
	(e_)->text = (unsigned char *) (text_);				\
	(e_)->rtext = (unsigned char *) (rtext_);			\
	(e_)->action_id = (action_id_);					\
	(e_)->func = (func_);						\
	(e_)->data = (void *) (data_);					\
	(e_)->flags = (flags_);						\
	(e_)->hotkey_state = (hotkey_state_);				\
	(e_)->hotkey_pos = (hotkey_pos_);				\
} while (0)


struct menu {
	struct window *win;	/* The terminal window the menu lives in */

	struct menu_item *items;/* The items in the menu */
	int size;		/* The number of menu items */

	int selected;		/* The current selected item. -1 means none */
	int first, last;	/* The first and last visible menu items */

	struct box box;		/* The visible area of the menu */
	int parent_x, parent_y;	/* The coordinates of the parent window */

	int hotkeys;		/* Whether to check and display hotkeys */
#ifdef CONFIG_NLS
	int lang;		/* For keeping the hotkey cache in sync */
#endif

	/* The private menu data that is passed as the 3. arg to the
	 * menu items' menu_func_T handler */
	void *data;
};


struct menu_item *new_menu(enum menu_item_flags);

void
add_to_menu(struct menu_item **mi, unsigned char *text, unsigned char *rtext,
	    enum main_action action_id, menu_func_T func, void *data,
	    enum menu_item_flags flags);

#define add_menu_separator(menu) \
	add_to_menu(menu, "", NULL, ACT_MAIN_NONE, NULL, NULL, NO_SELECT)

/* Implies that the action will be handled by do_action() */
#define add_menu_action(menu, text, action_id) \
	add_to_menu(menu, text, NULL, action_id, NULL, NULL, NO_FLAG)

void do_menu(struct terminal *, struct menu_item *, void *, int);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int, int);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);
void deselect_mainmenu(struct terminal *term, struct menu *menu);

#endif
