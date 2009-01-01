/* Hotkeys handling. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/hotkey.h"
#include "bfu/menu.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/memory.h"


/* Return position (starting at 1) of the first tilde in text,
 * or 0 if not found. */
static inline int
find_hotkey_pos(unsigned char *text)
{
	if (text && *text) {
		unsigned char *p = strchr(text, '~');

		if (p) return (int) (p - text) + 1;
	}

	return 0;
}

void
init_hotkeys(struct terminal *term, struct menu *menu)
{
	struct menu_item *mi;

#ifdef CONFIG_DEBUG
	/* hotkey debugging */
	if (menu->hotkeys) {
		struct menu_item *used_hotkeys[255];

		memset(used_hotkeys, 0, sizeof(used_hotkeys));

		foreach_menu_item(mi, menu->items) {
			unsigned char *text = mi->text;

			if (!mi_has_left_text(mi)) continue;
			if (mi_text_translate(mi)) text = _(text, term);
			if (!*text) continue;

			if (mi->hotkey_state != HKS_CACHED && !mi->hotkey_pos)
				mi->hotkey_pos = find_hotkey_pos(text);

			/* Negative value for hotkey_pos means the key is already
			 * used by another entry. We mark it to be able to highlight
			 * this hotkey in menus. --Zas */
			if (mi->hotkey_pos) {
				struct menu_item **used = &used_hotkeys[toupper(text[mi->hotkey_pos])];

				if (*used) {
					mi->hotkey_pos = -mi->hotkey_pos;
					if ((*used)->hotkey_pos > 0)
						(*used)->hotkey_pos = -(*used)->hotkey_pos;
				}

				*used = mi;
				mi->hotkey_state = HKS_CACHED;
			}
		}
	}
#endif

	foreach_menu_item(mi, menu->items) {
		if (!menu->hotkeys) {
			mi->hotkey_pos = 0;
			mi->hotkey_state = HKS_IGNORE;
		} else if (mi->hotkey_state != HKS_CACHED
			   && !mi->hotkey_pos) {
			unsigned char *text = mi->text;

			if (!mi_has_left_text(mi)) continue;
			if (mi_text_translate(mi)) text = _(text, term);
			if (!*text) continue;

			mi->hotkey_pos = find_hotkey_pos(text);

			if (mi->hotkey_pos)
				mi->hotkey_state = HKS_CACHED;
		}
	}
}

#ifdef CONFIG_NLS
void
clear_hotkeys_cache(struct menu *menu)
{
	struct menu_item *item;

	foreach_menu_item(item, menu->items) {
		item->hotkey_state = menu->hotkeys ? HKS_SHOW : HKS_IGNORE;
		item->hotkey_pos = 0;
	}
}
#endif

void
refresh_hotkeys(struct terminal *term, struct menu *menu)
{
#ifdef CONFIG_NLS
 	if (current_language != menu->lang) {
		clear_hotkeys_cache(menu);
		init_hotkeys(term, menu);
		menu->lang = current_language;
	}
#else
	init_hotkeys(term, menu);
#endif
}

static int
check_hotkeys_common(struct menu *menu, term_event_char_T hotkey, struct terminal *term,
		     int check_mode)
{
#ifdef CONFIG_UTF8
	unicode_val_T key = unicode_fold_label_case(hotkey);
	int codepage = get_terminal_codepage(term);
#else
	unsigned char key = toupper(hotkey);
#endif
	int i = menu->selected;
	int start;

	if (menu->size < 1) return 0;

	i %= menu->size;
	if (i < 0) i += menu->size;

	start = i;
	do {
		struct menu_item *item;
		unsigned char *text;
#ifdef CONFIG_UTF8
		unicode_val_T items_hotkey;
#endif
		int found;

		if (++i == menu->size) i = 0;

		item = &menu->items[i];

		if (!mi_has_left_text(item)) continue;

		text = item->text;
		if (mi_text_translate(item)) text = _(text, term);
		if (!text || !*text) continue;

		/* Change @text to point to the character that should
		 * be compared to @key.  */
		if (check_mode == 0) {
			/* Does the key (upcased) matches one of the
			 * hotkeys in menu ? */
			int key_pos = item->hotkey_pos;

#ifdef CONFIG_DEBUG
			if (key_pos < 0) key_pos = -key_pos;
#endif
			if (!key_pos) continue;
			text += key_pos;
		} else {
			/* Does the key (upcased) matches first letter
			 * of menu item left text ? */
		}

		/* Compare @key to the character to which @text points.  */
#ifdef CONFIG_UTF8
		items_hotkey = cp_to_unicode(codepage, &text,
					     strchr(text, '\0'));
		/* items_hotkey can be UCS_NO_CHAR only if the text of
		 * the menu item is not in the expected codepage.  */
		assert(items_hotkey != UCS_NO_CHAR);
		if_assert_failed continue;
		found = (unicode_fold_label_case(items_hotkey) == key);
#else
		found = (toupper(*text) == key);
#endif

		if (found) {
			menu->selected = i;
			return 1;
		}

	} while (i != start);

	return 0;
}

/* Returns true if a hotkey was found in the menu, and set menu->selected. */
int
check_hotkeys(struct menu *menu, term_event_char_T key, struct terminal *term)
{
	return check_hotkeys_common(menu, key, term, 0);
}

/* Search if first letter of an entry in menu matches the key (caseless comp.).
 * It searchs in all entries, from selected entry to bottom and then from top
 * to selected entry.
 * It returns 1 if found and set menu->selected. */
int
check_not_so_hot_keys(struct menu *menu, term_event_char_T key, struct terminal *term)
{
	return check_hotkeys_common(menu, key, term, 1);
}
