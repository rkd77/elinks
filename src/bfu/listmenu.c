/* List menus functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listmenu.h"
#include "bfu/menu.h"
#include "session/session.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/string.h"
#include "viewer/text/form.h" /* selected_item() and get_current_state() */

/* TODO: massive cleanup, merging, code redundancy tracking between this file
 * and bfu/menu.c (and perhaps others.)
 * We should unify and clarify menu-related code. */

static int
menu_contains(struct menu_item *m, int f)
{
	if (m->func != do_select_submenu)
		return (long) m->data == f;

	foreach_menu_item (m, m->data) {
		if (menu_contains(m, f))
			return 1;
	}

	return 0;
}

void
do_select_submenu(struct terminal *term, void *menu_, void *ses_)
{
	struct menu_item *menu = menu_;
	struct session *ses = ses_;
	struct menu_item *m;
	int def = int_max(0, get_current_state(ses));
	int sel = 0;

	foreach_menu_item (m, menu) {
		if (menu_contains(m, def)) {
			sel = m - menu;
			break;
		}
	}

	do_menu_selected(term, menu, ses, sel, 0);
}

void
new_menu_item(struct list_menu *menu, unsigned char *name, int data, int fullname)
	/* name == NULL - up;	data == -1 - down */
{
	struct menu_item *new_menu_item = NULL; /* no uninitialized warnings */
	struct menu_item **items;
	size_t stack_size = menu->stack_size;

	if (!name) {
		menu->stack_size--;
		return;

	} else if (data != -1 && menu->stack_size == 0) {
		mem_free(name);
		return;
	}

	clr_spaces(name);
	if (!name[0]) {
		mem_free(name);
		name = stracpy(" ");
		if (!name) return;
	}

	if (data == -1) {
		int size = (menu->stack_size + 1) * sizeof(*menu->stack);
		struct menu_item **stack = mem_realloc(menu->stack, size);

		if (stack) {
			menu->stack = stack;
			new_menu_item = new_menu(NO_INTL);
		}

		if (!stack || !new_menu_item) {
			if (new_menu_item) mem_free(new_menu_item);
			mem_free(name);
			return;
		}

		/* Since we increment @stack_size use cached value */
		menu->stack[menu->stack_size++] = new_menu_item;

		if (menu->stack_size == 1) {
			mem_free(name);
			return;
		}
	}

	items = &menu->stack[stack_size - 1];

	if (data == -1) {
		add_to_menu(items, name, NULL, ACT_MAIN_NONE,
			    do_select_submenu,
			    new_menu_item, SUBMENU);
	} else {
		add_to_menu(items, name, NULL, ACT_MAIN_NONE,
			    selected_item,
			    (void *) (long) data, (fullname ? MENU_FULLNAME : 0));
	}

	if (stack_size >= 2) {
		struct menu_item *below = menu->stack[stack_size - 2];

		while (below->text) below++;
		below[-1].data = *items;
	}
}

void
init_menu(struct list_menu *menu)
{
	menu->stack_size = 0;
	menu->stack = NULL;
	new_menu_item(menu, stracpy(""), -1, 0);
}

/* TODO: merge with free_menu_items() in bfu/menu.h --Zas */
void
free_menu(struct menu_item *m) /* Grrr. Recursion */
{
	struct menu_item *mm;

	if (!m) return; /* XXX: Who knows... need to be verified */

	foreach_menu_item (mm, m) {
		mem_free_if(mm->text);
		if (mm->func == do_select_submenu) free_menu(mm->data);
	}

	mem_free(m);
}

struct menu_item *
detach_menu(struct list_menu *menu)
{
	struct menu_item *i = NULL;

	if (menu->stack) {
		if (menu->stack_size) i = menu->stack[0];
		mem_free(menu->stack);
	}

	return i;
}

void
destroy_menu(struct list_menu *menu)
{
	if (menu->stack) free_menu(menu->stack[0]);
	detach_menu(menu);
}

void
menu_labels(struct menu_item *items, unsigned char *base, unsigned char **lbls)
{
	struct menu_item *item;
	unsigned char *bs;

	foreach_menu_item (item, items) {
		bs = (item->flags & MENU_FULLNAME) ? (unsigned char *) ""
						   : base;
		bs = straconcat(bs, item->text, (unsigned char *) NULL);
		if (!bs) continue;

		if (item->func == do_select_submenu) {
			add_to_strn(&bs, " ");
			menu_labels(item->data, bs, lbls);
			mem_free(bs);
		} else {
			assert(item->func == selected_item);
			lbls[(long) item->data] = bs;
		}
	}
}

void
add_select_item(struct list_menu *menu, struct string *string,
		struct string *orig_string, unsigned char **value,
		int order, int dont_add)
{
	int pos = order - 1;

	assert(menu && string);

	if (!string->source) return;

	assert(value && pos >= 0);

	if (!value[pos])
		/* <select> values are not mangled by various encode_*()
		 * functions, therefore we need to store them in the
		 * original document encoding. */
		value[pos] = memacpy(orig_string->source, orig_string->length);

	if (dont_add) {
		done_string(string);
	} else {
		new_menu_item(menu, string->source, pos, 1);
		string->source = NULL;
		string->length = 0;
	}
	done_string(orig_string);
}
