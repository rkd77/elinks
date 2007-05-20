/* Menu system implementation. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/hotkey.h"
#include "bfu/inpfield.h"
#include "bfu/menu.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/kbd.h"
#include "terminal/mouse.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/memory.h"
#include "viewer/action.h"

/* Left and right main menu reserved spaces. */
#define L_MAINMENU_SPACE	2
#define R_MAINMENU_SPACE	2

/* Left and right padding spaces around labels in main menu. */
#define L_MAINTEXT_SPACE	1
#define R_MAINTEXT_SPACE	1

/* Spaces before and after right text of submenu. */
#define L_RTEXT_SPACE		1
#define R_RTEXT_SPACE		1

/* Spaces before and after left text of submenu. */
#define L_TEXT_SPACE		1
#define R_TEXT_SPACE		1

/* Border size in submenu. */
#define MENU_BORDER_SIZE	1


/* Types and structures */

/* Submenu indicator, displayed at right. */
static unsigned char m_submenu[] = ">>";
static int m_submenu_len = sizeof(m_submenu) - 1;

/* Prototypes */
static window_handler_T menu_handler;
static window_handler_T mainmenu_handler;
static void display_mainmenu(struct terminal *term, struct menu *menu);
static void set_menu_selection(struct menu *menu, int pos);


void
deselect_mainmenu(struct terminal *term, struct menu *menu)
{
	menu->selected = -1;
	del_from_list(menu->win);
	add_to_list_end(term->windows, menu->win);
}

static inline int
count_items(struct menu_item *items)
{
	int i = 0;

	if (items) {
		struct menu_item *item;

		foreach_menu_item (item, items) {
			i++;
		}
	}

	return i;
}

static void
free_menu_items(struct menu_item *items)
{
	struct menu_item *item;

	if (!items || !(items->flags & FREE_ANY)) return;

	/* Note that flags & FREE_DATA applies only when menu is aborted;
	 * it is zeroed when some menu field is selected. */

	foreach_menu_item (item, items) {
		if (item->flags & FREE_TEXT) mem_free_if(item->text);
		if (item->flags & FREE_RTEXT) mem_free_if(item->rtext);
		if (item->flags & FREE_DATA) mem_free_if(item->data);
	}

	mem_free(items);
}

void
do_menu_selected(struct terminal *term, struct menu_item *items,
		 void *data, int selected, int hotkeys)
{
	struct menu *menu = mem_calloc(1, sizeof(*menu));

	if (menu) {
		menu->selected = selected;
		menu->items = items;
		menu->data = data;
		menu->size = count_items(items);
		menu->hotkeys = hotkeys;
#ifdef CONFIG_NLS
		menu->lang = -1;
#endif
		refresh_hotkeys(term, menu);
		add_window(term, menu_handler, menu);
	} else {
		free_menu_items(items);
	}
}

void
do_menu(struct terminal *term, struct menu_item *items, void *data, int hotkeys)
{
	do_menu_selected(term, items, data, 0, hotkeys);
}

static void
select_menu_item(struct terminal *term, struct menu_item *it, void *data)
{
	/* We save these values due to delete_window() call below. */
	menu_func_T func = it->func;
	void *it_data = it->data;
	enum main_action action_id = it->action_id;

	if (!mi_is_selectable(it)) return;

	if (!mi_is_submenu(it)) {
		/* Don't free data! */
		it->flags &= ~FREE_DATA;

		while (!list_empty(term->windows)) {
			struct window *win = term->windows.next;

			if (win->handler != menu_handler
			    && win->handler != mainmenu_handler)
				break;

			if (win->handler == mainmenu_handler) {
				deselect_mainmenu(term, win->data);
				redraw_terminal(term);
			} else
				delete_window(win);
		}
	}

	if (action_id != ACT_MAIN_NONE && !func) {
		struct session *ses = data;

		do_action(ses, action_id, 1);
		return;
	}

	assertm(func != NULL, "No menu function");
	if_assert_failed return;

	func(term, it_data, data);
}

static inline void
select_menu(struct terminal *term, struct menu *menu)
{
	if (menu->selected < 0 || menu->selected >= menu->size)
		return;

	select_menu_item(term, &menu->items[menu->selected], menu->data);
}

/* Get desired width for left text in menu item, accounting spacing. */
static int
get_menuitem_text_width(struct terminal *term, struct menu_item *mi)
{
	unsigned char *text;

	if (!mi_has_left_text(mi)) return 0;

	text = mi->text;
	if (mi_text_translate(mi))
		text = _(text, term);

	if (!text[0]) return 0;

#ifdef CONFIG_UTF8
	if (term->utf8_cp)
		return L_TEXT_SPACE + utf8_ptr2cells(text, NULL)
		       - !!mi->hotkey_pos + R_TEXT_SPACE;
	else
#endif /* CONFIG_UTF8 */
		return L_TEXT_SPACE + strlen(text)
		       - !!mi->hotkey_pos + R_TEXT_SPACE;
}

/* Get desired width for right text in menu item, accounting spacing. */
static int
get_menuitem_rtext_width(struct terminal *term, struct menu_item *mi)
{
	int rtext_width = 0;

	if (mi_is_submenu(mi)) {
		rtext_width = L_RTEXT_SPACE + m_submenu_len + R_RTEXT_SPACE;

	} else if (mi->action_id != ACT_MAIN_NONE) {
		struct string keystroke;

		if (init_string(&keystroke)) {
			add_keystroke_action_to_string(&keystroke, mi->action_id, KEYMAP_MAIN);
			rtext_width = L_RTEXT_SPACE + keystroke.length + R_RTEXT_SPACE;
			done_string(&keystroke);
		}

	} else if (mi_has_right_text(mi)) {
		unsigned char *rtext = mi->rtext;

		if (mi_rtext_translate(mi))
			rtext = _(rtext, term);

		if (rtext[0])
			rtext_width = L_RTEXT_SPACE + strlen(rtext) + R_RTEXT_SPACE;
	}

	return rtext_width;
}

static int
get_menuitem_width(struct terminal *term, struct menu_item *mi, int max_width)
{
	int text_width = get_menuitem_text_width(term, mi);
	int rtext_width = get_menuitem_rtext_width(term, mi);

	int_upper_bound(&text_width, max_width);
	int_upper_bound(&rtext_width, max_width - text_width);

	return text_width + rtext_width;
}

static void
count_menu_size(struct terminal *term, struct menu *menu)
{
	struct menu_item *item;
	int width = term->width - MENU_BORDER_SIZE * 2;
	int height = term->height - MENU_BORDER_SIZE * 2;
	int my = int_min(menu->size, height);
	int mx = 0;

	foreach_menu_item (item, menu->items) {
		int_lower_bound(&mx, get_menuitem_width(term, item, width));
	}

	set_box(&menu->box,
		menu->parent_x, menu->parent_y,
		mx + MENU_BORDER_SIZE * 2,
		my + MENU_BORDER_SIZE * 2);

	int_bounds(&menu->box.x, 0, width - mx);
	int_bounds(&menu->box.y, 0, height - my);
}

static void
scroll_menu(struct menu *menu, int steps, int wrap)
{
	int pos, start;
	int s = steps ? steps/abs(steps) : 1; /* Selectable item search direction. */

	/* menu->selected sometimes became -2 and caused infinite loops.
	 * That should no longer be possible. */
	assert(menu->selected >= -1);
	if_assert_failed return;

	if (menu->size <= 0) {
no_item:
		/* Menu is empty. */
		menu->selected = -1;
		menu->first = 0;
		return;
	}

	start = pos = menu->selected;

	if (!steps) {
		/* The caller wants us to check that menu->selected is
		 * actually selectable, and correct it if not. */
		steps = 1;
		if (pos >= 0)
			--pos;
	}

	while (steps) {
		pos += s, steps -= s;

		while (1) {
			if (start == pos) {
				goto select_item;
			} else if (pos >= menu->size && s == 1) {
				if (wrap) {
					pos = 0;
				} else {
					pos = menu->size - 1;
					goto select_item;
				}
			} else if (pos < 0 && s == -1) {
				if (wrap) {
					pos = menu->size - 1;
				} else {
					pos = 0;
					goto select_item;
				}
			} else if (!mi_is_selectable(&menu->items[pos])) {
				pos += s;
			} else {
				break;
			}

			if (start == -1) start = 0;
		}
	}

select_item:
	if (!mi_is_selectable(&menu->items[pos]))
		goto no_item;
	set_menu_selection(menu, pos);
}

/* Set menu->selected = pos, and adjust menu->first if needed.
 * This neither redraws the menu nor runs the item's action.
 * The caller must ensure that menu->items[pos] is selectable. */
static void
set_menu_selection(struct menu *menu, int pos)
{
	int height, scr_i;

	assert(pos >= 0 && pos < menu->size);
	assert(mi_is_selectable(&menu->items[pos]));
	if_assert_failed return;

	menu->selected = pos;

	height = int_max(1, menu->box.height - MENU_BORDER_SIZE * 2);

	/* The rest is not needed for horizontal menus like the mainmenu.
	 * FIXME: We need a better way to figure out which menus are horizontal and
	 * which are vertical (normal) --jonas */
	if (height == 1) return;

	scr_i = int_min((height - 1) / 2, SCROLL_ITEMS);

	int_bounds(&menu->first, menu->selected - height + scr_i + 1, menu->selected - scr_i);
	int_bounds(&menu->first, 0, menu->size - height);
}

/* width - number of standard terminal cells to be displayed (text + whitespace
 *         separators). For double-width glyph width == 2.
 * len - length of text in bytes */
static inline void
draw_menu_left_text(struct terminal *term, unsigned char *text, int len,
		    int x, int y, int width, struct color_pair *color)
{
	int w = width - (L_TEXT_SPACE + R_TEXT_SPACE);
	int max_len;

	if (w <= 0) return;

	if (len < 0) len = strlen(text);
	if (!len) return;

#ifdef CONFIG_UTF8
	if (term->utf8_cp) {
		max_len = utf8_cells2bytes(text, w, NULL);
		if (max_len <= 0)
			return;
	} else
#endif /* CONFIG_UTF8 */
		max_len = w;

	if (len > max_len) len = max_len;

	draw_text(term, x + L_TEXT_SPACE, y, text, len, 0, color);
}


static inline void
draw_menu_left_text_hk(struct terminal *term, unsigned char *text,
		       int hotkey_pos, int x, int y, int width,
		       struct color_pair *color, int selected)
{
	struct color_pair *hk_color = get_bfu_color(term, "menu.hotkey.normal");
	struct color_pair *hk_color_sel = get_bfu_color(term, "menu.hotkey.selected");
	enum screen_char_attr hk_attr = get_opt_bool("ui.dialogs.underline_hotkeys")
				      ? SCREEN_ATTR_UNDERLINE : 0;
	unsigned char c;
	int xbase = x + L_TEXT_SPACE;
	int w = width - (L_TEXT_SPACE + R_TEXT_SPACE);
	int hk_state = 0;
#ifdef CONFIG_UTF8
	unsigned char *text2, *end;
#endif

#ifdef CONFIG_DEBUG
	/* For redundant hotkeys highlighting. */
	int double_hk = 0;

	if (hotkey_pos < 0) hotkey_pos = -hotkey_pos, double_hk = 1;
#endif

	if (!hotkey_pos || w <= 0) return;

	if (selected) {
		struct color_pair *tmp = hk_color;

		hk_color = hk_color_sel;
		hk_color_sel = tmp;
	}

#ifdef CONFIG_UTF8
	if (term->utf8_cp) goto utf8;
#endif /* CONFIG_UTF8 */

	for (x = 0; x - !!hk_state < w && (c = text[x]); x++) {
		if (!hk_state && x == hotkey_pos - 1) {
			hk_state = 1;
			continue;
		}

		if (hk_state == 1) {
#ifdef CONFIG_DEBUG
			draw_char(term, xbase + x - 1, y, c, hk_attr,
				  (double_hk ? hk_color_sel : hk_color));
#else
			draw_char(term, xbase + x - 1, y, c, hk_attr, hk_color);
#endif /* CONFIG_DEBUG */
			hk_state = 2;
		} else {
			draw_char(term, xbase + x - !!hk_state, y, c, 0, color);
		}
	}
	return;

#ifdef CONFIG_UTF8
utf8:
	end = strchr(text, '\0');
	text2 = text;
	for (x = 0; x - !!hk_state < w && *text2; x++) {
		unicode_val_T data;

		data = utf8_to_unicode(&text2, end);
		if (!hk_state && (int)(text2 - text) == hotkey_pos) {
			hk_state = 1;
			continue;
		}
		if (hk_state == 1) {
			if (unicode_to_cell(data) == 2) {
				if (x < w && xbase + x < term->width) {
#ifdef CONFIG_DEBUG
					draw_char(term, xbase + x - 1, y,
						  data, hk_attr,
						  (double_hk ? hk_color_sel
						             : hk_color));
#else
					draw_char(term, xbase + x - 1, y,
						  data, hk_attr, hk_color);
#endif /* CONFIG_DEBUG */
					x++;
					draw_char(term, xbase + x - 1, y,
						  UCS_NO_CHAR, 0, hk_color);
				} else {
					draw_char(term, xbase + x - 1, y,
						  UCS_ORPHAN_CELL, 0, hk_color);
				}
			} else {
#ifdef CONFIG_DEBUG
				draw_char(term, xbase + x - 1, y,
					  data, hk_attr,
					  (double_hk ? hk_color_sel
					   	     : hk_color));
#else
				draw_char(term, xbase + x - 1, y,
					  data, hk_attr, hk_color);
#endif /* CONFIG_DEBUG */
			}
			hk_state = 2;
		} else {
			if (unicode_to_cell(data) == 2) {
				if (x - !!hk_state + 1 < w &&
				    xbase + x - !!hk_state + 1 < term->width) {
					draw_char(term, xbase + x - !!hk_state,
						  y, data, 0, color);
					x++;
					draw_char(term, xbase + x - !!hk_state,
						  y, UCS_NO_CHAR, 0, color);
				} else {
					draw_char(term, xbase + x - !!hk_state,
						  y, UCS_ORPHAN_CELL, 0, color);
				}
			} else {
				draw_char(term, xbase + x - !!hk_state,
					  y, data, 0, color);
			}
		}

	}
#endif /* CONFIG_UTF8 */
}

static inline void
draw_menu_right_text(struct terminal *term, unsigned char *text, int len,
		     int x, int y, int width, struct color_pair *color)
{
	int w = width - (L_RTEXT_SPACE + R_RTEXT_SPACE);

	if (w <= 0) return;

	if (len < 0) len = strlen(text);
	if (!len) return;
	if (len > w) len = w;

	x += w - len + L_RTEXT_SPACE + L_TEXT_SPACE;

	draw_text(term, x, y, text, len, 0, color);
}

static void
display_menu(struct terminal *term, struct menu *menu)
{
	struct color_pair *normal_color = get_bfu_color(term, "menu.normal");
	struct color_pair *selected_color = get_bfu_color(term, "menu.selected");
	struct color_pair *frame_color = get_bfu_color(term, "menu.frame");
	struct box box;
	int p;
	int menu_height;

	set_box(&box,
		menu->box.x + MENU_BORDER_SIZE,
		menu->box.y + MENU_BORDER_SIZE,
		int_max(0, menu->box.width - MENU_BORDER_SIZE * 2),
		int_max(0, menu->box.height - MENU_BORDER_SIZE * 2));

	draw_box(term, &box, ' ', 0, normal_color);
	draw_border(term, &box, frame_color, 1);

	if (get_opt_bool("ui.dialogs.shadows")) {
		/* Draw shadow */
		draw_shadow(term, &menu->box,
			    get_bfu_color(term, "dialog.shadow"), 2, 1);
#ifdef CONFIG_UTF8
		if (term->utf8_cp)
			fix_dwchar_around_box(term, &box, 1, 2, 1);
#endif /* CONFIG_UTF8 */
	}
#ifdef CONFIG_UTF8
	else if (term->utf8_cp)
		fix_dwchar_around_box(term, &box, 1, 0, 0);
#endif /* CONFIG_UTF8 */

	menu_height = box.height;
	box.height = 1;

	for (p = menu->first;
	     p < menu->size && p < menu->first + menu_height;
	     p++, box.y++) {
		struct color_pair *color = normal_color;
		struct menu_item *mi = &menu->items[p];
		int selected = (p == menu->selected);

#ifdef CONFIG_DEBUG
		/* Sanity check. */
		if (mi_is_end_of_menu(mi))
			INTERNAL("Unexpected end of menu [%p:%d]", mi, p);
#endif

		if (selected) {
			/* This entry is selected. */
			color = selected_color;

			set_cursor(term, box.x, box.y, 1);
			set_window_ptr(menu->win, menu->box.x + menu->box.width, box.y);
			draw_box(term, &box, ' ', 0, color);
		}

		if (mi_is_horizontal_bar(mi)) {
			/* Horizontal separator */
			draw_border_char(term, menu->box.x, box.y,
					 BORDER_SRTEE, frame_color);

			draw_box(term, &box, BORDER_SHLINE,
				 SCREEN_ATTR_FRAME, frame_color);

			draw_border_char(term, box.x + box.width, box.y,
					 BORDER_SLTEE, frame_color);

			continue;
		}

		if (mi_has_left_text(mi)) {
			int l = mi->hotkey_pos;
			unsigned char *text = mi->text;

			if (mi_text_translate(mi))
				text = _(text, term);

			if (!mi_is_selectable(mi))
				l = 0;

			if (l) {
				draw_menu_left_text_hk(term, text, l,
						       box.x, box.y, box.width, color,
						       selected);

			} else {
				draw_menu_left_text(term, text, -1,
						    box.x, box.y, box.width, color);
			}
		}

		if (mi_is_submenu(mi)) {
			draw_menu_right_text(term, m_submenu, m_submenu_len,
					     menu->box.x, box.y, box.width, color);
		} else if (mi->action_id != ACT_MAIN_NONE) {
			struct string keystroke;

#ifdef CONFIG_DEBUG
			/* Help to detect action + right text. --Zas */
			if (mi_has_right_text(mi)) {
				if (color == selected_color)
					color = normal_color;
				else
					color = selected_color;
			}
#endif /* CONFIG_DEBUG */

			if (init_string(&keystroke)) {
				add_keystroke_action_to_string(&keystroke,
							       mi->action_id,
							       KEYMAP_MAIN);
				draw_menu_right_text(term, keystroke.source,
						     keystroke.length,
						     menu->box.x, box.y,
						     box.width, color);
				done_string(&keystroke);
			}

		} else if (mi_has_right_text(mi)) {
			unsigned char *rtext = mi->rtext;

			if (mi_rtext_translate(mi))
				rtext = _(rtext, term);

			if (*rtext) {
				/* There's a right text, so print it */
				draw_menu_right_text(term, rtext, -1,
						     menu->box.x,
						     box.y, box.width, color);
			}
		}
	}

	redraw_from_window(menu->win);
}


#ifdef CONFIG_MOUSE
static void
menu_mouse_handler(struct menu *menu, struct term_event *ev)
{
	struct window *win = menu->win;
	int scroll_direction = 1;

	switch (get_mouse_button(ev)) {
		case B_WHEEL_UP:
			scroll_direction = -1;
			/* Fall thru */
		case B_WHEEL_DOWN:
			if (check_mouse_action(ev, B_DOWN)) {
				scroll_menu(menu, scroll_direction, 1);
				display_menu(win->term, menu);
			}
			return;
	}

	if (!check_mouse_position(ev, &menu->box)) {
		if (check_mouse_action(ev, B_DOWN)) {
			delete_window_ev(win, ev);

		} else {
			struct window *w1;
			struct window *end = (struct window *) &win->term->windows;

			for (w1 = win; w1 != end; w1 = w1->next) {
				struct menu *m1;

				if (w1->handler == mainmenu_handler) {
					if (!ev->info.mouse.y)
						delete_window_ev(win, ev);
					break;
				}

				if (w1->handler != menu_handler) break;

				m1 = w1->data;

				if (check_mouse_position(ev, &m1->box)) {
					delete_window_ev(win, ev);
					break;
				}
			}
		}

	} else {
		int sel = ev->info.mouse.y - menu->box.y - 1 + menu->first;

		if (sel >= 0 && sel < menu->size
		    && mi_is_selectable(&menu->items[sel])) {
			set_menu_selection(menu, sel);
			display_menu(win->term, menu);
			select_menu(win->term, menu);
		}
	}
}
#endif

#define DIST 5

static void
menu_page_up(struct menu *menu)
{
	int current = int_max(0, int_min(menu->selected, menu->size - 1));
	int step;
	int i;
	int next_sep = 0;

	for (i = current - 1; i > 0; i--)
		if (mi_is_horizontal_bar(&menu->items[i])) {
			next_sep = i;
			break;
		}

	step = current - next_sep + 1;
	int_bounds(&step, 0, int_min(current, DIST));

	scroll_menu(menu, -step, 0);
}

static void
menu_page_down(struct menu *menu)
{
	int current = int_max(0, int_min(menu->selected, menu->size - 1));
	int step;
	int i;
	int next_sep = menu->size - 1;

	for (i = current + 1; i < menu->size; i++)
		if (mi_is_horizontal_bar(&menu->items[i])) {
			next_sep = i;
			break;
		}

	step = next_sep - current + 1;
	int_bounds(&step, 0, int_min(menu->size - 1 - current, DIST));

	scroll_menu(menu, step, 0);
}

#undef DIST

static inline int
search_menu_item(struct menu_item *item, unsigned char *buffer,
		 struct terminal *term)
{
	unsigned char *text, *match;

	/* set_menu_selection asserts selectability. */
	if (!mi_has_left_text(item) || !mi_is_selectable(item)) return 0;

	text = mi_text_translate(item) ? _(item->text, term) : item->text;

	/* Crap. We have to remove the hotkey markers '~' */
	text = stracpy(text);
	if (!text) return 0;

	match = strchr(text, '~');
	if (match)
		memmove(match, match + 1, strlen(match));

	match = strcasestr(text, buffer);
	mem_free(text);

	return !!match;
}

static enum input_line_code
menu_search_handler(struct input_line *line, int action_id)
{
	struct menu *menu = line->data;
	struct terminal *term = menu->win->term;
	unsigned char *buffer = line->buffer;
	struct window *win;
	int pos = menu->selected;
	int start;
	int direction;

	switch (action_id) {
	case ACT_EDIT_REDRAW:
		return INPUT_LINE_PROCEED;

	case ACT_EDIT_ENTER:
		/* XXX: The input line dialog window is above the menu window.
		 * Remove it from the top, so that select_menu() will correctly
		 * remove all the windows it has to and then readd it. */
		win = term->windows.next;
		del_from_list(win);
		select_menu(term, menu);
		add_to_list(term->windows, win);
		return INPUT_LINE_CANCEL;

	case ACT_EDIT_PREVIOUS_ITEM:
		pos--;
		direction = -1;
		break;

	case ACT_EDIT_NEXT_ITEM:
		pos++;
	default:
		direction = 1;
	}

	/* If there is nothing to match with don't start searching */
	if (!*buffer) return INPUT_LINE_PROCEED;

	pos %= menu->size;

	start = pos;
	do {
		struct menu_item *item = &menu->items[pos];

		if (search_menu_item(item, buffer, term)) {
			set_menu_selection(menu, pos);
			display_menu(term, menu);
			return INPUT_LINE_PROCEED;
		}

		pos += direction;

		if (pos == menu->size) pos = 0;
		else if (pos < 0) pos = menu->size - 1;
	} while (pos != start);

	return INPUT_LINE_CANCEL;
}

static void
search_menu(struct menu *menu)
{
	struct terminal *term = menu->win->term;
	struct window *current_tab = get_current_tab(term);
	struct session *ses = current_tab ? current_tab->data : NULL;
	unsigned char *prompt = _("Search menu/", term);

	if (menu->size < 1 || !ses) return;

	input_field_line(ses, prompt, menu, NULL, menu_search_handler);
}

static void
menu_kbd_handler(struct menu *menu, struct term_event *ev)
{
	struct window *win = menu->win;
	enum menu_action action_id = kbd_action(KEYMAP_MENU, ev, NULL);
	int s = 0;

	switch (action_id) {
		case ACT_MENU_LEFT:
		case ACT_MENU_RIGHT:
			if (list_has_next(win->term->windows, win)
			    && win->next->handler == mainmenu_handler) {
				struct window *next_win = win->next;

				delete_window_ev(win, ev);

				select_menu(next_win->term, next_win->data);

				return;
			}

			if (action_id == ACT_MENU_RIGHT)
				goto enter;

			delete_window(win);
			return;

		case ACT_MENU_UP:
			scroll_menu(menu, -1, 1);
			break;

		case ACT_MENU_DOWN:
			scroll_menu(menu, 1, 1);
			break;

		case ACT_MENU_HOME:
			scroll_menu(menu, -menu->selected, 0);
			break;

		case ACT_MENU_END:
			scroll_menu(menu, menu->size - menu->selected - 1, 0);
			break;

		case ACT_MENU_PAGE_UP:
			menu_page_up(menu);
			break;

		case ACT_MENU_PAGE_DOWN:
			menu_page_down(menu);
			break;

		case ACT_MENU_ENTER:
		case ACT_MENU_SELECT:
			goto enter;

		case ACT_MENU_SEARCH:
			search_menu(menu);
			break;

		case ACT_MENU_CANCEL:
			if (list_has_next(win->term->windows, win)
			    && win->next->handler == mainmenu_handler)
				delete_window_ev(win, ev);
			else
				delete_window_ev(win, NULL);

			return;

		default:
		{
			term_event_key_T key = get_kbd_key(ev);

			if (is_kbd_fkey(key)
			    || check_kbd_modifier(ev, KBD_MOD_ALT)) {
				delete_window_ev(win, ev);
				return;
			}

			if (!check_kbd_label_key(ev))
				break;

			s = check_hotkeys(menu, key, win->term);

			if (s || check_not_so_hot_keys(menu, key, win->term))
				scroll_menu(menu, 0, 1);
		}
	}

	display_menu(win->term, menu);
	if (s) {
enter:
		select_menu(win->term, menu);
	}
}

static void
menu_handler(struct window *win, struct term_event *ev)
{
	struct menu *menu = win->data;

	menu->win = win;

	switch (ev->ev) {
		case EVENT_INIT:
		case EVENT_RESIZE:
			get_parent_ptr(win, &menu->parent_x, &menu->parent_y);
		case EVENT_REDRAW:
			count_menu_size(win->term, menu);
			/* do_menu sets menu->selected = 0.  If that
			 * item isn't actually selectable, correct
			 * menu->selected here. */
			scroll_menu(menu, 0, 1);
			display_menu(win->term, menu);
			break;

		case EVENT_MOUSE:
#ifdef CONFIG_MOUSE
			menu_mouse_handler(menu, ev);
#endif /* CONFIG_MOUSE */
			break;

		case EVENT_KBD:
			menu_kbd_handler(menu, ev);
			break;

		case EVENT_ABORT:
			free_menu_items(menu->items);
			break;
	}
}


void
do_mainmenu(struct terminal *term, struct menu_item *items,
	    void *data, int sel)
{
	int init = 0;
	struct menu *menu;
	struct window *win;

	if (!term->main_menu) {
		term->main_menu = mem_calloc(1, sizeof(*menu));
		if (!term->main_menu) return;
		init = 1;
	}

	menu = term->main_menu;
	menu->selected = (sel == -1 ? 0 : sel);
	menu->items = items;
	menu->data = data;
	menu->size = count_items(items);
	menu->hotkeys = 1;

#ifdef CONFIG_NLS
	clear_hotkeys_cache(menu);
#endif
	init_hotkeys(term, menu);
	if (init) {
		add_window(term, mainmenu_handler, menu);
		win = menu->win;
		/* This should be fine because add_window will call
		 * mainmenu_handler which will assign the window to menu->win.
		 */
		assert(win);
		deselect_mainmenu(term, menu);
	} else {
		foreach (win, term->windows) {
			if (win->data == menu) {
				del_from_list(win);
				add_to_list(term->windows, win);
				display_mainmenu(term, menu);
				break;
			}
		}
	}

	if (sel != -1) {
		select_menu(term, menu);
	}
}

static void
display_mainmenu(struct terminal *term, struct menu *menu)
{
	struct color_pair *normal_color = get_bfu_color(term, "menu.normal");
	struct color_pair *selected_color = get_bfu_color(term, "menu.selected");
	int p = 0;
	int i;
	struct box box;

	/* FIXME: menu horizontal scrolling do not work well yet, we need to cache
	 * menu items width and recalculate them only when needed (ie. language change)
	 * instead of looping and calculate them each time. --Zas */

	/* Try to make current selected menu entry visible. */
	if (menu->selected < menu->first) {
		int num_items_offscreen = menu->selected - menu->first;

		menu->first += num_items_offscreen;
		menu->last += num_items_offscreen;
	} else if (menu->selected > menu->last) {
		int num_items_offscreen = menu->last - menu->selected;

		menu->first -= num_items_offscreen;
		menu->last -= num_items_offscreen;
	}

	if (menu->last <= 0)
		menu->last = menu->size - 1;

	int_bounds(&menu->last, 0, menu->size - 1);
	int_bounds(&menu->first, 0, menu->last);

	set_box(&box, 0, 0, term->width, 1);
	draw_box(term, &box, ' ', 0, normal_color);

	if (menu->first != 0) {
		box.width = L_MAINMENU_SPACE;
		draw_box(term, &box, '<', 0, normal_color);
	}

	p += L_MAINMENU_SPACE;

	for (i = menu->first; i < menu->size; i++) {
		struct menu_item *mi = &menu->items[i];
		struct color_pair *color = normal_color;
		unsigned char *text = mi->text;
		int l = mi->hotkey_pos;
		int textlen;
		int selected = (i == menu->selected);
		int screencnt;

		if (mi_text_translate(mi))
			text = _(text, term);

		textlen = strlen(text) - !!l;
#ifdef CONFIG_UTF8
		if (term->utf8_cp)
			screencnt = utf8_ptr2cells(text, NULL) - !!l;
		else
#endif /* CONFIG_UTF8 */
			screencnt = textlen;

		if (selected) {
			color = selected_color;
			box.x = p;
#ifdef CONFIG_UTF8
			if (term->utf8_cp)
				box.width = L_MAINTEXT_SPACE + L_TEXT_SPACE
					+ screencnt
					+ R_TEXT_SPACE + R_MAINTEXT_SPACE;
			else
#endif /* CONFIG_UTF8 */
				box.width = L_MAINTEXT_SPACE + L_TEXT_SPACE
					+ textlen
					+ R_TEXT_SPACE + R_MAINTEXT_SPACE;

			draw_box(term, &box, ' ', 0, color);
			set_cursor(term, p, 0, 1);
			set_window_ptr(menu->win, p, 1);
		}

		p += L_MAINTEXT_SPACE;

		if (l) {
			draw_menu_left_text_hk(term, text, l,
					       p, 0, textlen + R_TEXT_SPACE + L_TEXT_SPACE,
					       color, selected);
		} else {
			draw_menu_left_text(term, text, textlen,
					    p, 0, textlen + R_TEXT_SPACE + L_TEXT_SPACE,
					    color);
		}

		p += screencnt;

		if (p >= term->width - R_MAINMENU_SPACE)
			break;

		p += R_MAINTEXT_SPACE + R_TEXT_SPACE + L_TEXT_SPACE;
	}

	menu->last = i - 1;
	int_lower_bound(&menu->last, menu->first);
	if (menu->last < menu->size - 1) {
#ifdef CONFIG_UTF8
		if (term->utf8_cp) {
			struct screen_char *schar;

			schar = get_char(term, term->width - R_MAINMENU_SPACE, 0);
			/* Is second cell of double-width char on the place where
			 * first char of the R_MAINMENU_SPACE will be displayed? */
			if (schar->data == UCS_NO_CHAR) {
				/* Replace double-width char with UCS_ORPHAN_CELL. */
				schar++;
				draw_char_data(term, term->width - R_MAINMENU_SPACE - 1,
					       0, UCS_ORPHAN_CELL);
			}
		}
#endif

		set_box(&box,
			term->width - R_MAINMENU_SPACE, 0,
			R_MAINMENU_SPACE, 1);
		draw_box(term, &box, '>', 0, normal_color);
	}

	redraw_from_window(menu->win);
}


#ifdef CONFIG_MOUSE
static void
mainmenu_mouse_handler(struct menu *menu, struct term_event *ev)
{
	struct window *win = menu->win;
	struct menu_item *item;
	int scroll = 0;

	if (check_mouse_wheel(ev))
		return;

	/* Mouse was clicked outside the mainmenu bar */
	if (ev->info.mouse.y) {
		if (check_mouse_action(ev, B_DOWN)) {
			deselect_mainmenu(win->term, menu);
			display_mainmenu(win->term, menu);

		}
		return;
	}

	/* First check if the mouse button was pressed in the side of the
	 * terminal and simply scroll one step in that direction else iterate
	 * through the menu items to see if it was pressed on a label. */
	if (ev->info.mouse.x < L_MAINMENU_SPACE) {
		scroll = -1;

	} else if (ev->info.mouse.x >= win->term->width - R_MAINMENU_SPACE) {
		scroll = 1;

	} else {
		int p = L_MAINMENU_SPACE;

		/* We don't initialize to menu->first here, since it breaks
		 * horizontal scrolling using mouse in some cases. --Zas */
		foreach_menu_item (item, menu->items) {
			unsigned char *text = item->text;

			if (!mi_has_left_text(item)) continue;

			if (mi_text_translate(item))
				text = _(item->text, win->term);

			/* The label width is made up of a little padding on
			 * the sides followed by the text width substracting
			 * one char if it has hotkeys (the '~' char) */
			p += L_MAINTEXT_SPACE + L_TEXT_SPACE
			  + strlen(text) - !!item->hotkey_pos
			  + R_TEXT_SPACE + R_MAINTEXT_SPACE;

			if (ev->info.mouse.x < p) {
				scroll = (item - menu->items) - menu->selected;
				break;
			}
		}
	}

	if (scroll) {
		scroll_menu(menu, scroll, 1);
		display_mainmenu(win->term, menu);
	}

	/* We need to select the menu item even if we didn't scroll
	 * apparently because we will delete any drop down menus
	 * in the clicking process. */
	select_menu(win->term, menu);
}
#endif

static void
mainmenu_kbd_handler(struct menu *menu, struct term_event *ev)
{
	struct window *win = menu->win;
	enum menu_action action_id = kbd_action(KEYMAP_MENU, ev, NULL);

	switch (action_id) {
	case ACT_MENU_ENTER:
	case ACT_MENU_DOWN:
	case ACT_MENU_UP:
	case ACT_MENU_PAGE_UP:
	case ACT_MENU_PAGE_DOWN:
	case ACT_MENU_SELECT:
		select_menu(win->term, menu);
		return;

	case ACT_MENU_HOME:
		scroll_menu(menu, -menu->selected, 0);
		break;

	case ACT_MENU_END:
		scroll_menu(menu, menu->size - menu->selected - 1, 0);
		break;

	case ACT_MENU_NEXT_ITEM:
	case ACT_MENU_PREVIOUS_ITEM:
		/* This is pretty western centric since `what is next'?
		 * Anyway we cycle clockwise by resetting the action ... */
		action_id = (action_id == ACT_MENU_NEXT_ITEM)
		       ? ACT_MENU_RIGHT : ACT_MENU_LEFT;
		/* ... and then letting left/right handling take over. */

	case ACT_MENU_LEFT:
	case ACT_MENU_RIGHT:
		scroll_menu(menu, action_id == ACT_MENU_LEFT ? -1 : 1, 1);
		break;

	case ACT_MENU_REDRAW:
		/* Just call display_mainmenu() */
		break;

	default:
		/* Fallback to see if any hotkey matches the pressed key */
		if (check_kbd_label_key(ev)
		    && check_hotkeys(menu, get_kbd_key(ev), win->term)) {
			display_mainmenu(win->term, menu);
			select_menu(win->term, menu);

			return;
		}

	case ACT_MENU_CANCEL:
		deselect_mainmenu(win->term, menu);
		break;
	}

	/* Redraw the menu */
	display_mainmenu(win->term, menu);
}

static void
mainmenu_handler(struct window *win, struct term_event *ev)
{
	struct menu *menu = win->data;

	menu->win = win;

	switch (ev->ev) {
		case EVENT_INIT:
		case EVENT_RESIZE:
		case EVENT_REDRAW:
			display_mainmenu(win->term, menu);
			break;

		case EVENT_MOUSE:
#ifdef CONFIG_MOUSE
			mainmenu_mouse_handler(menu, ev);
#endif /* CONFIG_MOUSE */
			break;

		case EVENT_KBD:
			mainmenu_kbd_handler(menu, ev);
			break;

		case EVENT_ABORT:
			break;
	}
}

/* For dynamic menus the last (cleared) item is used to mark the end. */
#define realloc_menu_items(mi_, size) \
	mem_align_alloc(mi_, size, (size) + 2, 0xF)

struct menu_item *
new_menu(enum menu_item_flags flags)
{
	struct menu_item *mi = NULL;

	if (realloc_menu_items(&mi, 0)) mi->flags = flags;

	return mi;
}

void
add_to_menu(struct menu_item **mi, unsigned char *text, unsigned char *rtext,
	    enum main_action action_id, menu_func_T func, void *data,
	    enum menu_item_flags flags)
{
	int n = count_items(*mi);
	/* XXX: Don't clear the last and special item. */
	struct menu_item *item = realloc_menu_items(mi, n + 1);

	if (!item) return;

	item += n;

	/* Shift current last item by one place. */
	copy_struct(item + 1, item);

	/* Setup the new item. All menu items share the item_free value. */
	SET_MENU_ITEM(item, text, rtext, action_id, func, data,
		      item->flags | flags, HKS_SHOW, 0);
}

#undef L_MAINMENU_SPACE
#undef R_MAINMENU_SPACE
#undef L_MAINTEXT_SPACE
#undef R_MAINTEXT_SPACE
#undef L_RTEXT_SPACE
#undef R_RTEXT_SPACE
#undef L_TEXT_SPACE
#undef R_TEXT_SPACE
#undef MENU_BORDER_SIZE
