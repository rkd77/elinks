/* Dialog box implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "main/timer.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"


static window_handler_T dialog_func;

struct dialog_data *
do_dialog(struct terminal *term, struct dialog *dlg,
	  struct memory_list *ml)
{
	struct dialog_data *dlg_data;

	dlg_data = mem_calloc(1, sizeof(*dlg_data) +
			      sizeof(struct widget_data) * dlg->number_of_widgets);
	if (!dlg_data) {
		/* Worry not: freeml() checks whether its argument is NULL. */
		freeml(ml);
		return NULL;
	}

	dlg_data->dlg = dlg;
	dlg_data->number_of_widgets = dlg->number_of_widgets;
	dlg_data->ml = ml;
	add_window(term, dialog_func, dlg_data);

	return dlg_data;
}

static void cycle_widget_focus(struct dialog_data *dlg_data, int direction);

static void
update_all_widgets(struct dialog_data *dlg_data)
{
	struct widget_data *widget_data;

	/* Iterate backwards rather than forwards so that listboxes are drawn
	 * last, which means that they can grab the cursor. Yes, 'tis hacky. */
	foreach_widget_back(dlg_data, widget_data) {
		display_widget(dlg_data, widget_data);
	}
}

void
redraw_dialog(struct dialog_data *dlg_data, int layout)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *title_color;

	if (layout) {
		dlg_data->dlg->layouter(dlg_data);
		/* This might not be the best place. We need to be able
		 * to make focusability of widgets dynamic so widgets
		 * like scrollable text don't receive focus when there
		 * is nothing to scroll. */
		if (!widget_is_focusable(selected_widget(dlg_data)))
			cycle_widget_focus(dlg_data, 1);
	}

	if (!dlg_data->dlg->layout.only_widgets) {
		struct box box;

		set_box(&box,
			dlg_data->box.x + (DIALOG_LEFT_BORDER + 1),
			dlg_data->box.y + (DIALOG_TOP_BORDER + 1),
			dlg_data->box.width - 2 * (DIALOG_LEFT_BORDER + 1),
			dlg_data->box.height - 2 * (DIALOG_TOP_BORDER + 1));

		draw_border(term, &box, get_bfu_color(term, "dialog.frame"), DIALOG_FRAME);

		assert(dlg_data->dlg->title);

		title_color = get_bfu_color(term, "dialog.title");
		if (title_color && box.width > 2) {
			unsigned char *title = dlg_data->dlg->title;
			int titlelen = strlen(title);
			int titlecells = titlelen;
			int x, y;

#ifdef CONFIG_UTF8
			if (term->utf8_cp)
				titlecells = utf8_ptr2cells(title,
							    &title[titlelen]);
#endif /* CONFIG_UTF8 */

			titlecells = int_min(box.width - 2, titlecells);

#ifdef CONFIG_UTF8
			if (term->utf8_cp)
				titlelen = utf8_cells2bytes(title, titlecells,
							    NULL);
#endif /* CONFIG_UTF8 */

			x = (box.width - titlecells) / 2 + box.x;
			y = box.y - 1;


			draw_text(term, x - 1, y, " ", 1, 0, title_color);
			draw_text(term, x, y, title, titlelen, 0, title_color);
			draw_text(term, x + titlecells, y, " ", 1, 0,
				  title_color);
		}
	}

	update_all_widgets(dlg_data);

	redraw_from_window(dlg_data->win);
}

static void
select_dlg_item(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	select_widget(dlg_data, widget_data);

	if (widget_data->widget->ops->select)
		widget_data->widget->ops->select(dlg_data, widget_data);
}

static const struct widget_ops *const widget_type_to_ops[] = {
	&checkbox_ops,
	&field_ops,
	&field_pass_ops,
	&button_ops,
	&listbox_ops,
	&text_ops,
};

static struct widget_data *
init_widget(struct dialog_data *dlg_data, int i)
{
	struct widget_data *widget_data = &dlg_data->widgets_data[i];

	memset(widget_data, 0, sizeof(*widget_data));
	widget_data->widget = &dlg_data->dlg->widgets[i];

	if (widget_data->widget->datalen) {
		widget_data->cdata = mem_alloc(widget_data->widget->datalen);
		if (!widget_data->cdata) {
			return NULL;
		}
		memcpy(widget_data->cdata,
		       widget_data->widget->data,
		       widget_data->widget->datalen);
	}

	widget_data->widget->ops = widget_type_to_ops[widget_data->widget->type];

	if (widget_has_history(widget_data)) {
		init_list(widget_data->info.field.history);
		widget_data->info.field.cur_hist =
			(struct input_history_entry *) &widget_data->info.field.history;
	}

	if (widget_data->widget->ops->init)
		widget_data->widget->ops->init(dlg_data, widget_data);

	return widget_data;
}

void
select_widget(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct widget_data *previously_selected_widget;

	previously_selected_widget = selected_widget(dlg_data);

	dlg_data->selected_widget_id = widget_data - dlg_data->widgets_data;

	display_widget(dlg_data, previously_selected_widget);
	display_widget(dlg_data, widget_data);
}


struct widget_data *
select_widget_by_id(struct dialog_data *dlg_data, int i)
{
	struct widget_data *widget_data;

	if (i >= dlg_data->number_of_widgets)
		return NULL;

	widget_data = &dlg_data->widgets_data[i];
	select_widget(dlg_data, widget_data);

	return widget_data;
}

static void
cycle_widget_focus(struct dialog_data *dlg_data, int direction)
{
	int prev_selected = dlg_data->selected_widget_id;
	struct widget_data *previously_selected_widget;

	previously_selected_widget = selected_widget(dlg_data);

	do {
		dlg_data->selected_widget_id += direction;

		if (dlg_data->selected_widget_id >= dlg_data->number_of_widgets)
			dlg_data->selected_widget_id = 0;
		else if (dlg_data->selected_widget_id < 0)
			dlg_data->selected_widget_id = dlg_data->number_of_widgets - 1;

	} while (!widget_is_focusable(selected_widget(dlg_data))
		 && dlg_data->selected_widget_id != prev_selected);

	display_widget(dlg_data, previously_selected_widget);
	display_widget(dlg_data, selected_widget(dlg_data));
	redraw_from_window(dlg_data->win);
}

static void
dialog_ev_init(struct dialog_data *dlg_data)
{
	int i;

	/* TODO: foreachback_widget() */
	for (i = dlg_data->number_of_widgets - 1; i >= 0; i--) {
		struct widget_data *widget_data;

		widget_data = init_widget(dlg_data, i);

		/* Make sure the selected widget is focusable */
		if (widget_data
		    && widget_is_focusable(widget_data))
			dlg_data->selected_widget_id = i;
	}
}

#ifdef CONFIG_MOUSE
static void
dialog_ev_mouse(struct dialog_data *dlg_data)
{
	struct widget_data *widget_data;

	foreach_widget(dlg_data, widget_data) {
		if (widget_data->widget->ops->mouse
		    && widget_data->widget->ops->mouse(dlg_data, widget_data)
		       == EVENT_PROCESSED)
			break;
	}
}
#endif /* CONFIG_MOUSE */

/* Look up for a button with matching flag. */
static void
select_button_by_flag(struct dialog_data *dlg_data, int flag)
{
	struct widget_data *widget_data;

	foreach_widget(dlg_data, widget_data) {
		if (widget_data->widget->type == WIDGET_BUTTON
		    && widget_data->widget->info.button.flags & flag) {
			select_dlg_item(dlg_data, widget_data);
			break;
		}
	}
}

/* Look up for a button with matching starting letter. */
static void
select_button_by_key(struct dialog_data *dlg_data)
{
	term_event_char_T key;
#ifdef CONFIG_UTF8
	int codepage;
#endif

	struct widget_data *widget_data;
	struct term_event *ev = dlg_data->term_event;

	if (!check_kbd_label_key(ev)) return;

#ifdef CONFIG_UTF8
	key = unicode_fold_label_case(get_kbd_key(ev));
	codepage = get_terminal_codepage(dlg_data->win->term);
#else
	key = toupper(get_kbd_key(ev));
#endif

	foreach_widget(dlg_data, widget_data) {
		int hk_pos;
		unsigned char *hk_ptr;
		term_event_char_T hk_char;

		if (widget_data->widget->type != WIDGET_BUTTON)
			continue;

		hk_ptr = widget_data->widget->text;
		if (!*hk_ptr)
			continue;

		/* We first try to match marked hotkey if there is
		 * one else we fallback to first character in button
		 * name. */
		hk_pos = widget_data->widget->info.button.hotkey_pos;
		if (hk_pos >= 0)
			hk_ptr += hk_pos + 1;

#ifdef CONFIG_UTF8
		hk_char = cp_to_unicode(codepage, &hk_ptr,
					strchr(hk_ptr, '\0'));
		/* hk_char can be UCS_NO_CHAR only if the text of the
		 * widget is not in the expected codepage.  */
		assert(hk_char != UCS_NO_CHAR);
		if_assert_failed continue;
		hk_char = unicode_fold_label_case(hk_char);
#else
		hk_char = toupper(*hk_ptr);
#endif

		if (hk_char == key) {
			select_dlg_item(dlg_data, widget_data);
			break;
		}
	}
}

static void
dialog_ev_kbd(struct dialog_data *dlg_data)
{
	struct widget_data *widget_data = selected_widget(dlg_data);
	const struct widget_ops *ops = widget_data->widget->ops;
	/* XXX: KEYMAP_EDIT ? --pasky */
	enum menu_action action_id;
	struct term_event *ev = dlg_data->term_event;

	/* First let the widget try out. */
	if (ops->kbd && ops->kbd(dlg_data, widget_data) == EVENT_PROCESSED)
		return;

	action_id = kbd_action(KEYMAP_MENU, ev, NULL);
	switch (action_id) {
	case ACT_MENU_SELECT:
		/* Can we select? */
		if (ops->select) {
			ops->select(dlg_data, widget_data);
		}
		break;
	case ACT_MENU_ENTER:
		/* Submit button. */
		if (ops->select) {
			ops->select(dlg_data, widget_data);
			break;
		}

		if (widget_is_textfield(widget_data)
		    || check_kbd_modifier(ev, KBD_MOD_CTRL)
		    || check_kbd_modifier(ev, KBD_MOD_ALT)) {
			select_button_by_flag(dlg_data, B_ENTER);
		}
		break;
	case ACT_MENU_CANCEL:
		/* Cancel button. */
		select_button_by_flag(dlg_data, B_ESC);
		break;
	case ACT_MENU_NEXT_ITEM:
	case ACT_MENU_DOWN:
	case ACT_MENU_RIGHT:
		/* Cycle focus. */
		cycle_widget_focus(dlg_data, 1);
		break;
	case ACT_MENU_PREVIOUS_ITEM:
	case ACT_MENU_UP:
	case ACT_MENU_LEFT:
		/* Cycle focus (reverse). */
		cycle_widget_focus(dlg_data, -1);
		break;
	case ACT_MENU_REDRAW:
		redraw_terminal_cls(dlg_data->win->term);
		break;
	default:
		select_button_by_key(dlg_data);
		break;
	}
}

static void
dialog_ev_abort(struct dialog_data *dlg_data)
{
	struct widget_data *widget_data;

	if (dlg_data->dlg->refresh) {
		struct dialog_refresh *refresh = dlg_data->dlg->refresh;

		kill_timer(&refresh->timer);
		mem_free(refresh);
	}

	if (dlg_data->dlg->abort)
		dlg_data->dlg->abort(dlg_data);

	foreach_widget(dlg_data, widget_data) {
		mem_free_if(widget_data->cdata);
		if (widget_has_history(widget_data))
			free_list(widget_data->info.field.history);
	}

	freeml(dlg_data->ml);
}

/* TODO: use EVENT_PROCESSED/EVENT_NOT_PROCESSED. */
static void
dialog_func(struct window *win, struct term_event *ev)
{
	struct dialog_data *dlg_data = win->data;

	dlg_data->win = win;
	dlg_data->term_event = ev;

	/* Look whether user event handlers can help us.. */
	if (dlg_data->dlg->handle_event &&
	    (dlg_data->dlg->handle_event(dlg_data) == EVENT_PROCESSED)) {
		return;
	}

	switch (ev->ev) {
		case EVENT_INIT:
			dialog_ev_init(dlg_data);
			/* fallback */
		case EVENT_RESIZE:
		case EVENT_REDRAW:
			redraw_dialog(dlg_data, 1);
			break;

		case EVENT_MOUSE:
#ifdef CONFIG_MOUSE
			dialog_ev_mouse(dlg_data);
#endif
			break;

		case EVENT_KBD:
			dialog_ev_kbd(dlg_data);
			break;

		case EVENT_ABORT:
			dialog_ev_abort(dlg_data);
			break;
	}
}

int
check_dialog(struct dialog_data *dlg_data)
{
	struct widget_data *widget_data;

	foreach_widget(dlg_data, widget_data) {
		if (widget_data->widget->type != WIDGET_CHECKBOX &&
		    !widget_is_textfield(widget_data))
			continue;

		if (widget_data->widget->handler &&
		    widget_data->widget->handler(dlg_data, widget_data)
		     == EVENT_NOT_PROCESSED) {
			select_widget(dlg_data, widget_data);
			redraw_dialog(dlg_data, 0);
			return 1;
		}
	}

	return 0;
}

widget_handler_status_T
cancel_dialog(struct dialog_data *dlg_data, struct widget_data *xxx)
{
	delete_window(dlg_data->win);
	return EVENT_PROCESSED;
}

int
update_dialog_data(struct dialog_data *dlg_data)
{
	struct widget_data *widget_data;

	foreach_widget(dlg_data, widget_data) {
		if (!widget_data->widget->datalen)
			continue;
		memcpy(widget_data->widget->data,
		       widget_data->cdata,
		       widget_data->widget->datalen);
	}

	return 0;
}

widget_handler_status_T
ok_dialog(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	done_handler_T *done = widget_data->widget->info.button.done;
	void *done_data = widget_data->widget->info.button.done_data;

	if (check_dialog(dlg_data)) return EVENT_NOT_PROCESSED;

	update_dialog_data(dlg_data);

	if (done) done(done_data);
	return cancel_dialog(dlg_data, widget_data);
}

/* Clear dialog fields (if widget has clear callback). */
widget_handler_status_T
clear_dialog(struct dialog_data *dlg_data, struct widget_data *xxx)
{
	struct widget_data *widget_data;

	foreach_widget(dlg_data, widget_data) {
		if (widget_data->widget->ops->clear)
			widget_data->widget->ops->clear(dlg_data, widget_data);
	}

	/* Move focus to the first widget. It helps with bookmark search dialog
	 * (and others). */
	select_widget_by_id(dlg_data, 0);

	redraw_dialog(dlg_data, 0);
	return EVENT_PROCESSED;
}


static void
format_widgets(struct terminal *term, struct dialog_data *dlg_data,
	       int x, int *y, int w, int h, int *rw, int format_only)
{
	struct widget_data *wdata = dlg_data->widgets_data;
	int widgets = dlg_data->number_of_widgets;

	/* TODO: Do something if (*y) gets > height. */
	for (; widgets > 0; widgets--, wdata++, (*y)++) {
		switch (wdata->widget->type) {
		case WIDGET_FIELD_PASS:
		case WIDGET_FIELD:
			dlg_format_field(term, wdata, x, y, w, rw, ALIGN_LEFT,
					 format_only);
			break;

		case WIDGET_LISTBOX:
			dlg_format_listbox(term, wdata, x, y, w, h, rw,
					   ALIGN_LEFT, format_only);
			break;

		case WIDGET_TEXT:
			dlg_format_text(term, wdata, x, y, w, rw, h,
					format_only);
			break;

		case WIDGET_CHECKBOX:
		{
			int group = widget_has_group(wdata);

			if (group > 0 && dlg_data->dlg->layout.float_groups) {
				int size;

				/* Find group size */
				for (size = 1; widgets > 0; size++, widgets--) {
					struct widget_data *next = &wdata[size];

					if (group != widget_has_group(next))
						break;
				}

				dlg_format_group(term, wdata, size, x, y, w, rw,
						 format_only);
				wdata += size - 1;

			} else {

				/* No horizontal space between checkboxes belonging to
				 * the same group. */
				dlg_format_checkbox(term, wdata, x, y, w, rw,
						    ALIGN_LEFT, format_only);
				if (widgets > 1
				    && group == widget_has_group(&wdata[1]))
					(*y)--;
			}
		}
			break;

		/* We assume that the buttons are all stuffed at the very end
		 * of the dialog. */
		case WIDGET_BUTTON:
			dlg_format_buttons(term, wdata, widgets,
					   x, y, w, rw, ALIGN_CENTER, format_only);
			return;
		}
	}
}

void
generic_dialog_layouter(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int w = dialog_max_width(term);
	int height = dialog_max_height(term);
	int x = 0, y, rw;

#ifdef CONFIG_UTF8
	if (term->utf8_cp)
		rw = int_min(w, utf8_ptr2cells(dlg_data->dlg->title, NULL));
	else
#endif /* CONFIG_UTF8 */
		rw = int_min(w, strlen(dlg_data->dlg->title));
	y = dlg_data->dlg->layout.padding_top ? 0 : -1;

	format_widgets(term, dlg_data, x, &y, w, height, &rw, 1);

	/* Update the width to respond to the required minimum width */
	if (dlg_data->dlg->layout.fit_datalen) {
		int_lower_bound(&rw, dlg_data->dlg->widgets->datalen);
		int_upper_bound(&w, rw);
	} else if (!dlg_data->dlg->layout.maximize_width) {
		w = rw;
	}

	draw_dialog(dlg_data, w, y);

	y = dlg_data->box.y + DIALOG_TB + dlg_data->dlg->layout.padding_top;
	x = dlg_data->box.x + DIALOG_LB;

	format_widgets(term, dlg_data, x, &y, w, height, NULL, 0);
}


void
draw_dialog(struct dialog_data *dlg_data, int width, int height)
{
	struct terminal *term = dlg_data->win->term;
	int dlg_width = int_min(term->width, width + 2 * DIALOG_LB);
	int dlg_height = int_min(term->height, height + 2 * DIALOG_TB);

	set_box(&dlg_data->box,
		(term->width - dlg_width) / 2, (term->height - dlg_height) / 2,
		dlg_width, dlg_height);

	draw_box(term, &dlg_data->box, ' ', 0,
		 get_bfu_color(term, "dialog.generic"));

	if (get_opt_bool("ui.dialogs.shadows")) {
		/* Draw shadow */
		draw_shadow(term, &dlg_data->box,
			    get_bfu_color(term, "dialog.shadow"), 2, 1);
#ifdef CONFIG_UTF8
		if (term->utf8_cp)
			fix_dwchar_around_box(term, &dlg_data->box, 0, 2, 1);
#endif /* CONFIG_UTF8 */
	}
#ifdef CONFIG_UTF8
	else if (term->utf8_cp)
		fix_dwchar_around_box(term, &dlg_data->box, 0, 0, 0);
#endif /* CONFIG_UTF8 */
}

/* Timer callback for @refresh->timer.  As explained in @install_timer,
 * this function must erase the expired timer ID from all variables.  */
static void
do_refresh_dialog(struct dialog_data *dlg_data)
{
	struct dialog_refresh *refresh = dlg_data->dlg->refresh;
	enum dlg_refresh_code refresh_code;

	assert(refresh && refresh->handler);

	refresh_code = refresh->handler(dlg_data, refresh->data);

	if (refresh_code == REFRESH_CANCEL
	    || refresh_code == REFRESH_STOP) {
		refresh->timer = TIMER_ID_UNDEF;
		if (refresh_code == REFRESH_CANCEL)
			cancel_dialog(dlg_data, NULL);
		return;
	}

	/* We want dialog_has_refresh() to be true while drawing
	 * so we can not set the timer to -1. */
	if (refresh_code == REFRESH_DIALOG) {
		redraw_dialog(dlg_data, 1);
	}

	install_timer(&refresh->timer, RESOURCE_INFO_REFRESH,
		      (void (*)(void *)) do_refresh_dialog, dlg_data);
	/* The expired timer ID has now been erased.  */
}

void
refresh_dialog(struct dialog_data *dlg_data, dialog_refresh_handler_T handler, void *data)
{
	struct dialog_refresh *refresh = dlg_data->dlg->refresh;

	if (!refresh) {
		refresh = mem_calloc(1, sizeof(*refresh));
		if (!refresh) return;

		dlg_data->dlg->refresh = refresh;

	} else {
		kill_timer(&refresh->timer);
	}

	refresh->handler = handler;
	refresh->data = data;
	install_timer(&refresh->timer, RESOURCE_INFO_REFRESH,
		      (void (*)(void *)) do_refresh_dialog, dlg_data);
}
