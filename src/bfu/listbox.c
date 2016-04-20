/* Listbox widget implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/lists.h"


#define VERTICAL_LISTBOX_MARGIN 3

void
add_dlg_listbox(struct dialog *dlg, void *box_data)
{
	struct widget *widget = &dlg->widgets[dlg->number_of_widgets++];

	widget->type = WIDGET_LISTBOX;
	widget->data = box_data;
}

struct listbox_data *
get_listbox_widget_data(struct widget_data *widget_data)
{
	assert(widget_data->widget->type == WIDGET_LISTBOX);
	return ((struct listbox_data *) widget_data->widget->data);
}

/* Layout for generic boxes */
void
dlg_format_listbox(struct dialog_data *dlg_data,
		   struct widget_data *widget_data,
	           int x, int *y, int w, int max_height, int *rw,
	           enum format_align align, int format_only)
{
	int min, optimal_h, height;

	/* Height bussiness follows: */

	/* This is only weird heuristic, it could scale well I hope. */
	optimal_h = max_height * 7 / 10 - VERTICAL_LISTBOX_MARGIN;
	min = get_opt_int("ui.dialogs.listbox_min_height", NULL);

	if (max_height - VERTICAL_LISTBOX_MARGIN < min) {
		/* Big trouble: can't satisfy even the minimum :-(. */
		height = max_height - VERTICAL_LISTBOX_MARGIN;
	} else if (optimal_h < min) {
		height = min;
	} else {
		height = optimal_h;
	}

	set_box(&widget_data->box, x, *y, w, height);
	(*y) += height;
	if (rw) *rw = w;
}


/*
 *,item00->prev
 *|item00->root = NULL  ,item10->prev
 *|item00->child <----->|item10->root
 *|item00->next <-.     |item10->child [<->]
 *|               |     `item10->next
 *|item01->prev <-'
 *|item01->root = NULL
 *|item01->child [<->]
 *|item01->next <-.
 *|               |
 *|item02->prev <-'
 *|item02->root = NULL  ,item11->prev
 *|item02->child <----->|item11->root         ,item20->prev
 *`item02->next       \ |item11->child <----->|item20->root
 *                    | |item11->next <-.     |item20->child [<->]
 *                    | |               |     `item20->next
 *                    | |item12->prev <-'
 *                    `-|item12->root
 *                    | |item12->child [<->]
 *                    | |item12->next <-.
 *                    | |               |
 *                    | |item13->prev <-'
 *                    `-|item13->root         ,item21->prev
 *                      |item13->child <----->|item21->root
 *                      `item13->next         |item21->child [<->]
 *                                            `item21->next
 *
 */

/* Traverse a hierarchic tree from @item by @offset items, calling @fn,
 * if it is not NULL, on each item traversed (that is, each of the items
 * that we move _through_; this means from the passed @item up to,
 * but not including, the returned item).
 *
 * @offset may be negative to indicate that we should traverse upwards.
 *
 * Besides the current item, @fn is also passed @d, which is otherwise unused
 * by traverse_listbox_items_list, and a pointer to @offset, which @fn can set
 * to 0 to stop traversal or to other values to change the direction in which
 * or the number of items over which we will traverse.
 *
 * @fn should return 1 if it freed its passed item, or return 0 otherwise.
 *
 * If the passed @offset is zero, we set @offset to 1 and traverse thru
 * the list (down) until either we reach the end or @fn sets @offset to 0. */
/* From the box structure, we should use only 'items' here. */
struct listbox_item *
traverse_listbox_items_list(struct listbox_item *item, struct listbox_data *box,
			    int offset, int follow_visible,
			    int (*fn)(struct listbox_item *, void *, int *),
			    void *d)
{
	struct listbox_item *visible_item = item;
	int levmove = 0;
	int stop = 0;
	int infinite = !offset;

	if (!item) return NULL;

	if (infinite)
		offset = 1;

	while (offset && !stop) {
		/* We need to cache these. Or what will happen if something
		 * will free us item too early? However, we rely on item
		 * being at least NULL in that case. */
		/* There must be no orphaned listbox_items. No free()d roots
		 * and no dangling children. */
#define	item_cache(item) \
	do { \
		croot = box->ops->get_root(item); cprev = item->prev; cnext = item->next; \
	} while (0)
		struct listbox_item *croot, *cprev, *cnext;

		item_cache(item);

		if (fn && (!follow_visible || item->visible)) {
			if (fn(item, d, &offset)) {
				/* We was free()d! Let's try to carry on w/ the
				 * cached coordinates. */
				item = NULL;
			}
			if (!offset) {
				infinite = 0; /* safety (matches) */
				continue;
			}
		}

		if (offset > 0) {
			/* Otherwise we climb back up when last item in root
			 * is a folder. */
			struct listbox_item *cragsman = NULL;

			/* Direction DOWN. */

			if (!infinite) offset--;

			if (item && !list_empty(item->child) && item->expanded
			    && (!follow_visible || item->visible)) {
				/* Descend to children. */
				item = item->child.next;
				item_cache(item);
				goto done_down;
			}

			while (croot
			       && (void *) cnext == &croot->child) {
				/* Last item in a non-root list, climb to your
				 * root. */
				if (!cragsman) cragsman = item;
				item = croot;
				item_cache(item);
			}

			if (!croot && (!cnext || (void *) cnext == box->items)) {
				/* Last item in the root list, quit.. */
				stop = 1;
				if (cragsman) {
					/* ..and fall back where we were. */
					item = cragsman;
					item_cache(item);
				}
			}

			/* We're not at the end of anything, go on. */
			if (!stop) {
				item = cnext;
				item_cache(item);
			}

done_down:
			if (!item || (follow_visible && !item->visible)) {
				offset++;
			} else {
				visible_item = item;
			}

		} else {
			/* Direction UP. */

			if (!infinite) offset++;

			if (croot
			    && (void *) cprev == &croot->child) {
				/* First item in a non-root list, climb to your
				 * root. */
				item = croot;
				item_cache(item);
				levmove = 1;
			}

			if (!croot && (void *) cprev == box->items) {
				/* First item in the root list, quit. */
				stop = 1;
				levmove = 1;
			}

			/* We're not at the start of anything, go on. */
			if (!levmove && !stop) {
				item = cprev;
				item_cache(item);

				while (item && !list_empty(item->child)
					&& item->expanded
					&& (!follow_visible || item->visible)) {
					/* Descend to children. */
					item = item->child.prev;
					item_cache(item);
				}
			} else {
				levmove = 0;
			}

			if (!item || (follow_visible && !item->visible)) {
				offset--;
			} else {
				visible_item = item;
			}
		}
#undef item_cache
	}

	return visible_item;
}

static int
calc_dist(struct listbox_item *item, void *data_, int *offset)
{
	int *item_offset = data_;

	if (*offset < 0)
		--*item_offset;
	else if (*offset > 0)
		++*item_offset;

	return 0;
}

/* Moves the selected item by [dist] items. If [dist] is out of the current
 * range, the selected item is moved to the extreme (ie, the top or bottom) */
void
listbox_sel_move(struct widget_data *widget_data, int dist)
{
	struct listbox_data *box = get_listbox_widget_data(widget_data);

	if (list_empty(*box->items)) return;

	if (!box->top) box->top = box->items->next;
	if (!box->sel) box->sel = box->top;

	/* We want to have these visible if possible. */
	if (box->top && !box->top->visible) {
		box->top = traverse_listbox_items_list(box->top, box,
				1, 1, NULL, NULL);
		box->sel = box->top;
	}

	if (!dist && !box->sel->visible) dist = 1;

	if (dist) {
		box->sel = traverse_listbox_items_list(box->sel, box, dist, 1,
		                                       calc_dist,
		                                       &box->sel_offset);
		/* box->sel_offset becomes the offset of the new box->sel
		 * from box->top. */
	}

	if (box->sel_offset < 0) {
		/* We must scroll up. */
		box->sel_offset = 0;
		box->top = box->sel;
	} else if (box->sel_offset >= widget_data->box.height) {
		/* We must scroll down. */
		box->sel_offset = widget_data->box.height - 1;
		box->top = traverse_listbox_items_list(box->sel, box,
					   1 - widget_data->box.height,
					   1, NULL, NULL);
	}
}

static int
test_search(struct listbox_item *item, void *data_, int *offset)
{
	struct listbox_context *listbox_context = data_;

	if (item != listbox_context->item)
		listbox_context->offset++;
	else
		*offset = 0;

	return 0;
}

static int
listbox_item_offset(struct listbox_data *box, struct listbox_item *item)
{
	struct listbox_context ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.item = item;
	ctx.offset = 0;

	traverse_listbox_items_list(box->items->next, box, 0, 1, test_search, &ctx);

	return ctx.offset;
}

void
listbox_sel(struct widget_data *widget_data, struct listbox_item *item)
{
	struct listbox_data *box = get_listbox_widget_data(widget_data);

	listbox_sel_move(widget_data,
	                 listbox_item_offset(box, item)
	                  - listbox_item_offset(box, box->sel));
}


/* Takes care about rendering of each listbox item. */
static int
display_listbox_item(struct listbox_item *item, void *data_, int *offset)
{
	struct listbox_context *data = data_;
	int len; /* Length of the current text field. */
	struct color_pair *tree_color, *text_color;
	int depth = item->depth + 1;
	int d;
	int x, y;

	tree_color = get_bfu_color(data->term, "menu.normal");
	if (item == data->box->sel) {
		text_color = get_bfu_color(data->term, "menu.selected");

	} else if (item->marked) {
		text_color = get_bfu_color(data->term, "menu.marked");

	} else {
		text_color = tree_color;
	}

	y = data->widget_data->box.y + data->offset;
	for (d = 0; d < depth - 1; d++) {
		struct listbox_item *root = item;
		struct listbox_item *child = item;
		int i, x;

		for (i = depth - d; i; i--) {
			child = root;
			if (root) root = data->box->ops->get_root(root);
		}

		/* XXX */
		x = data->widget_data->box.x + d * 5;
		draw_text(data->term, x, y, "     ", 5, 0, tree_color);

		if (root ? root->child.prev == child
			 : data->box->items->prev == child)
			continue; /* We were the last branch. */

		draw_border_char(data->term, x + 1, y, BORDER_SVLINE, tree_color);
	}

	if (depth) {
		enum border_char str[5] =
			{ 32, BORDER_SRTEE, BORDER_SHLINE, BORDER_SHLINE, 32 };
		int i;

		switch (item->type) {
		case BI_LEAF:
		case BI_SEPARATOR:
		{
			const struct listbox_item *prev;

			prev = traverse_listbox_items_list(item, data->box,
			                                   -1, 1, NULL, NULL);

			if (item == prev) {
				/* There is no visible item before @item, so it
				 * must be the first item in the listbox. */
				str[1] = BORDER_SULCORNER;
			} else {
				const struct listbox_item *next;

				next = traverse_listbox_items_list(item,
				                  data->box, 1, 1, NULL, NULL);

				if (item == next
				    || item->depth != next->depth) {
					/* There is no visible item after @item
					 * at the same depth, so it must be the
					 * last in its folder. */
					str[1] = BORDER_SDLCORNER;
				}
			}
			break;
		}
		case BI_FOLDER:
			str[0] = '[';
			str[1] = (item->expanded) ? '-' : '+';
			str[2] = ']';
			break;
		default:
			INTERNAL("Unknown item type");
			break;
		}

		if (item->marked) str[4] = '*';

		x = data->widget_data->box.x + (depth - 1) * 5;
		for (i = 0; i < 5; i++) {
			draw_border_char(data->term, x + i, y, str[i], tree_color);
		}
	}

	x = data->widget_data->box.x + depth * 5;

	if (item->type == BI_SEPARATOR) {
		int i;
		int width = data->widget_data->box.width - depth * 5;

		for (i = 0; i < width; i++) {
			draw_border_char(data->term, x + i, y, BORDER_SHLINE, text_color);
		}

	} else if (data->box->ops && data->box->ops->draw) {
		int width = data->widget_data->box.width - depth * 5;

		data->box->ops->draw(item, data, x, y, width);

	} else {
		unsigned char *text;
		const struct listbox_ops *ops = data->box->ops;
		int len_bytes;

		assert(ops && ops->get_info);

		text = ops->get_text(item, data->term);
		if (!text) return 0;

		len = strlen(text);
		int_upper_bound(&len, int_max(0, data->widget_data->box.width - depth * 5));
#ifdef CONFIG_UTF8
		if (data->term->utf8_cp)
			len_bytes = utf8_cells2bytes(text, len, NULL);
		else
#endif /* CONFIG_UTF8 */
			len_bytes = len;

		draw_text(data->term, x, y, text, len_bytes, 0, text_color);

		mem_free(text);
	}

	if (item == data->box->sel) {
		/* For blind users: */
		x = data->widget_data->box.x + 5 + item->depth * 5;
		set_cursor(data->term, x, y, 1);
		set_window_ptr(data->dlg_data->win, x, y);
	}

	data->offset++;

	return 0;
}

/* Displays a dialog box */
static widget_handler_status_T
display_listbox(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_listbox_widget_data(widget_data);
	struct listbox_context data;

	listbox_sel_move(widget_data, 0);

	draw_box(term, &widget_data->box, ' ', 0,
		 get_bfu_color(term, "menu.normal"));

	memset(&data, 0, sizeof(data));
	data.term = term;
	data.widget_data = widget_data;
	data.box = box;
	data.dlg_data = dlg_data;

	traverse_listbox_items_list(box->top, box, widget_data->box.height,
				    1, display_listbox_item, &data);

	return EVENT_PROCESSED;
}

static int
check_old_state(struct listbox_item *item, void *info_, int *offset)
{
	struct listbox_data *box = info_;

	if (box->sel == item)
		box->sel = NULL;
	else if (box->top == item)
		box->top = NULL;

	if (!box->sel && !box->top)
		*offset = 0;

	return 0;
}

static widget_handler_status_T
init_listbox(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct hierbox_browser *browser = dlg_data->dlg->udata2;
	struct listbox_data *box = get_listbox_widget_data(widget_data);

	/* Try to restore the position from last time */
	if (!list_empty(browser->root.child) && browser->box_data.items) {
		copy_struct(box, &browser->box_data);

		traverse_listbox_items_list(browser->root.child.next, box, 0, 0,
					    check_old_state, box);

		box->sel = (!box->sel) ? browser->box_data.sel : NULL;
		box->top = (!box->top) ? browser->box_data.top : NULL;
		if (!box->sel) box->sel = box->top;
		if (!box->top) box->top = box->sel;
	}

	box->ops = browser->ops;
	box->items = &browser->root.child;

	add_to_list(browser->boxes, box);
	return EVENT_PROCESSED;
}

static widget_handler_status_T
mouse_listbox(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
#ifdef CONFIG_MOUSE
	struct listbox_data *box = get_listbox_widget_data(widget_data);
	struct term_event *ev = dlg_data->term_event;

	if (!list_empty(*box->items)) {
		if (!box->top) box->top = box->items->next;
		if (!box->sel) box->sel = box->top;
	}

	if (check_mouse_action(ev, B_DOWN)) {
		struct widget_data *dlg_item = dlg_data->widgets_data;

		switch (get_mouse_button(ev)) {
			case B_WHEEL_DOWN:
				listbox_sel_move(dlg_item, 1);
				display_widget(dlg_data, dlg_item);
				return EVENT_PROCESSED;

			case B_WHEEL_UP:
				listbox_sel_move(dlg_item, -1);
				display_widget(dlg_data, dlg_item);
				return EVENT_PROCESSED;
		}
	}

	if (check_mouse_wheel(ev))
		return EVENT_NOT_PROCESSED;

	if (check_mouse_position(ev, &widget_data->box)) {
		/* Clicked in the box. */
		int offset = ev->info.mouse.y - widget_data->box.y;

		box->sel_offset = offset;
		box->sel = offset ? traverse_listbox_items_list(box->top, box,
								offset, 1,
								NULL, NULL)
				  : box->top;

		if (box->sel && box->sel->type == BI_FOLDER) {
			int xdepth = widget_data->box.x + box->sel->depth * 5;
			int x = ev->info.mouse.x;

			if (x >= xdepth && x <= xdepth + 2)
				box->sel->expanded = !box->sel->expanded;
		}

		display_widget(dlg_data, widget_data);

		return EVENT_PROCESSED;
	}

#endif /* CONFIG_MOUSE */

	return EVENT_NOT_PROCESSED;
}

static widget_handler_status_T
do_kbd_listbox_action(enum menu_action action_id, struct dialog_data *dlg_data,
		      struct widget_data *widget_data)
{
	struct widget_data *dlg_item = dlg_data->widgets_data;

	switch (action_id) {
		case ACT_MENU_DOWN:
			listbox_sel_move(dlg_item, 1);
			display_widget(dlg_data, dlg_item);

			return EVENT_PROCESSED;

		case ACT_MENU_UP:
			listbox_sel_move(dlg_item, -1);
			display_widget(dlg_data, dlg_item);

			return EVENT_PROCESSED;

		case ACT_MENU_PAGE_DOWN:
		{
			struct listbox_data *box;

			box = get_listbox_widget_data(dlg_item);

			listbox_sel_move(dlg_item,
			                 2 * dlg_item->box.height
			                   - box->sel_offset - 1);

			display_widget(dlg_data, dlg_item);

			return EVENT_PROCESSED;
		}

		case ACT_MENU_PAGE_UP:
		{
			struct listbox_data *box;

			box = get_listbox_widget_data(dlg_item);

			listbox_sel_move(dlg_item,
			                 -dlg_item->box.height
			                  - box->sel_offset);

			display_widget(dlg_data, dlg_item);

			return EVENT_PROCESSED;
		}

		case ACT_MENU_HOME:
			listbox_sel_move(dlg_item, -INT_MAX);
			display_widget(dlg_data, dlg_item);

			return EVENT_PROCESSED;

		case ACT_MENU_END:
			listbox_sel_move(dlg_item, INT_MAX);
			display_widget(dlg_data, dlg_item);

			return EVENT_PROCESSED;

		case ACT_MENU_MARK_ITEM:
		{
			struct listbox_data *box;

			box = get_listbox_widget_data(dlg_item);
			if (box->sel) {
				box->sel->marked = !box->sel->marked;
				listbox_sel_move(dlg_item, 1);
			}
			display_widget(dlg_data, dlg_item);

			return EVENT_PROCESSED;
		}

		case ACT_MENU_DELETE:
		{
			struct listbox_data *box;

			box = get_listbox_widget_data(dlg_item);
			if (box->ops
			    && box->ops->delete_
			    && box->ops->can_delete)
				push_hierbox_delete_button(dlg_data,
							   widget_data);

			return EVENT_PROCESSED;
		}

		default:
			break;
	}

	return EVENT_NOT_PROCESSED;
}

static widget_handler_status_T
kbd_listbox(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct term_event *ev = dlg_data->term_event;

	/* Not a pure listbox, but you're not supposed to use this outside of
	 * the listbox browser anyway, so what.. */

	switch (ev->ev) {
		case EVENT_KBD:
		{
			enum menu_action action_id;

			action_id = kbd_action(KEYMAP_MENU, ev, NULL);
			return do_kbd_listbox_action(action_id, dlg_data, widget_data);
		}
		case EVENT_INIT:
		case EVENT_RESIZE:
		case EVENT_REDRAW:
		case EVENT_MOUSE:
		case EVENT_ABORT:
			break;
	}

	return EVENT_NOT_PROCESSED;
}

const struct widget_ops listbox_ops = {
	display_listbox,
	init_listbox,
	mouse_listbox,
	kbd_listbox,
	NULL,
	NULL,
};
