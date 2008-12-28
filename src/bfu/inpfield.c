/* Input field widget implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "config/kbdbind.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "osdep/osdep.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/memlist.h"
#include "util/memory.h"

#define INPUTFIELD_HEIGHT 1

#define INPUTFIELD_FLOATLABEL_PADDING 1

#define INPUTFIELD_FLOAT_SEPARATOR ":"
#define INPUTFIELD_FLOAT_SEPARATOR_LEN 1

void
add_dlg_field_do(struct dialog *dlg, enum widget_type type, unsigned char *label,
		 int min, int max, widget_handler_T *handler,
		 int datalen, void *data,
		 struct input_history *history, enum inpfield_flags flags)
{
	struct widget *widget = &dlg->widgets[dlg->number_of_widgets++];

	widget->type    = type;
	widget->text    = label;
	widget->handler = handler;
	widget->datalen = datalen;
	widget->data    = data;

	widget->info.field.history = history;
	widget->info.field.flags   = flags;
	widget->info.field.min     = min;
	widget->info.field.max     = max;
}

widget_handler_status_T
check_number(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct widget *widget = widget_data->widget;
	char *end;
	long l;

	errno = 0;
	l = strtol(widget_data->cdata, &end, 10);

	if (errno || !*widget_data->cdata || *end) {
		info_box(dlg_data->win->term, 0,
			 N_("Bad number"), ALIGN_CENTER,
			 N_("Number expected in field"));
		return EVENT_NOT_PROCESSED;
	}

	if (l < widget->info.field.min || l > widget->info.field.max) {
		info_box(dlg_data->win->term, MSGBOX_FREE_TEXT,
			 N_("Bad number"), ALIGN_CENTER,
			 msg_text(dlg_data->win->term,
				  N_("Number should be in the range from %d to %d."),
				  widget->info.field.min, widget->info.field.max));
		return EVENT_NOT_PROCESSED;
	}

	return EVENT_PROCESSED;
}

widget_handler_status_T
check_nonempty(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	unsigned char *p;

	for (p = widget_data->cdata; *p; p++)
		if (*p > ' ')
			return EVENT_PROCESSED;

	info_box(dlg_data->win->term, 0,
		 N_("Bad string"), ALIGN_CENTER,
		 N_("Empty string not allowed"));

	return EVENT_NOT_PROCESSED;
}

void
dlg_format_field(struct terminal *term,
		 struct widget_data *widget_data,
		 int x, int *y, int w, int *rw, enum format_align align, int format_only)
{
	static int max_label_width;
	static int *prev_y; /* Assert the uniqueness of y */	/* TODO: get rid of this !! --Zas */
	unsigned char *label = widget_data->widget->text;
	struct color_pair *text_color = NULL;
	int label_width = 0;
	int float_label = widget_data->widget->info.field.flags & (INPFIELD_FLOAT|INPFIELD_FLOAT2);

	if (label && *label && float_label) {
		label_width = strlen(label);
		if (prev_y == y) {
			int_lower_bound(&max_label_width, label_width);
		} else {
			max_label_width = label_width;
			prev_y = y;
		}

		/* Right align the floating label up against the
		 * input field */
		x += max_label_width - label_width;
		w -= max_label_width - label_width;
	}

	if (label && *label) {
		if (!format_only) text_color = get_bfu_color(term, "dialog.text");

		dlg_format_text_do(term, label, x, y, w, rw, text_color, ALIGN_LEFT, format_only);
	}

	/* XXX: We want the field and label on the same line if the terminal
	 * width allows it. */
	if (label && *label && float_label) {
		if (widget_data->widget->info.field.flags & INPFIELD_FLOAT) {
			(*y) -= INPUTFIELD_HEIGHT;
			dlg_format_text_do(term, INPUTFIELD_FLOAT_SEPARATOR,
					   x + label_width, y, w, rw,
					   text_color, ALIGN_LEFT, format_only);
			w -= INPUTFIELD_FLOAT_SEPARATOR_LEN + INPUTFIELD_FLOATLABEL_PADDING;
			x += INPUTFIELD_FLOAT_SEPARATOR_LEN + INPUTFIELD_FLOATLABEL_PADDING;
		}

		/* FIXME: Is 5 chars for input field enough? --jonas */
		if (label_width < w - 5) {
			(*y) -= INPUTFIELD_HEIGHT;
			w -= label_width;
			x += label_width;
		}
	}

	if (rw) int_lower_bound(rw, int_min(w, DIALOG_MIN_WIDTH));

	set_box(&widget_data->box, x, *y, w, INPUTFIELD_HEIGHT);

	(*y) += INPUTFIELD_HEIGHT;
}

static widget_handler_status_T
input_field_cancel(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *) = widget_data->widget->data;
	void *data = dlg_data->dlg->udata2;

	if (fn) fn(data);

	return cancel_dialog(dlg_data, widget_data);
}

static widget_handler_status_T
input_field_ok(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	void (*fn)(void *, unsigned char *) = widget_data->widget->data;
	void *data = dlg_data->dlg->udata2;
	unsigned char *text = dlg_data->widgets_data->cdata;

	if (check_dialog(dlg_data)) return EVENT_NOT_PROCESSED;

	if (widget_has_history(dlg_data->widgets_data))
		add_to_input_history(dlg_data->dlg->widgets->info.field.history,
				     text, 1);

	if (fn) fn(data, text);

	return cancel_dialog(dlg_data, widget_data);
}

void
input_field(struct terminal *term, struct memory_list *ml, int intl,
	    unsigned char *title,
	    unsigned char *text,
	    unsigned char *okbutton,
	    unsigned char *cancelbutton,
	    void *data, struct input_history *history, int l,
	    unsigned char *def, int min, int max,
	    widget_handler_T *check,
	    void (*fn)(void *, unsigned char *),
	    void (*cancelfn)(void *))
{
	struct dialog *dlg;
	unsigned char *field;

	if (intl) {
		title = _(title, term);
		text = _(text, term);
		okbutton = _(okbutton, term);
		cancelbutton = _(cancelbutton, term);
	}

#define INPUT_WIDGETS_COUNT 3
	dlg = calloc_dialog(INPUT_WIDGETS_COUNT, l);
	if (!dlg) return;

	/* @field is automatically cleared by calloc() */
	field = get_dialog_offset(dlg, INPUT_WIDGETS_COUNT);

	if (def) {
		int defsize = strlen(def) + 1;

		memcpy(field, def, (defsize > l) ? l - 1 : defsize);
	}

	dlg->title = title;
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.fit_datalen = 1;
	dlg->udata2 = data;

	add_dlg_field(dlg, text, min, max, check, l, field, history);

	add_dlg_button(dlg, okbutton, B_ENTER, input_field_ok, fn);
	add_dlg_button(dlg, cancelbutton, B_ESC, input_field_cancel, cancelfn);

	add_dlg_end(dlg, INPUT_WIDGETS_COUNT);

	add_to_ml(&ml, (void *) dlg, (void *) NULL);
	do_dialog(term, dlg, ml);
}

void
input_dialog(struct terminal *term, struct memory_list *ml,
	     unsigned char *title,
	     unsigned char *text,
	     void *data, struct input_history *history, int l,
	     unsigned char *def, int min, int max,
	     widget_handler_T *check,
	     void (*fn)(void *, unsigned char *),
	     void (*cancelfn)(void *))
{
	/* [gettext_accelerator_context(input_dialog)] */
	input_field(term, ml, 1, title, text, N_("~OK"), N_("~Cancel"),
		    data, history, l,
		    def, min, max,
		    check, fn, cancelfn);
}

static widget_handler_status_T
display_field_do(struct dialog_data *dlg_data, struct widget_data *widget_data,
		 int hide)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;
	int sel = is_selected_widget(dlg_data, widget_data);
#ifdef CONFIG_UTF8
	int len = 0, left = 0;
#endif /* CONFIG_UTF8 */

#ifdef CONFIG_UTF8
	if (term->utf8_cp) {
		unsigned char *t = widget_data->cdata;
		int p = widget_data->info.field.cpos;

		len = utf8_ptr2cells(t, &t[p]);
		int_bounds(&left, len - widget_data->box.width + 1, len);
		int_lower_bound(&left, 0);
		widget_data->info.field.vpos = utf8_cells2bytes(t, left, NULL);
	} else
#endif /* CONFIG_UTF8 */
	{
		int_bounds(&widget_data->info.field.vpos,
		   widget_data->info.field.cpos - widget_data->box.width + 1,
		   widget_data->info.field.cpos);
		int_lower_bound(&widget_data->info.field.vpos, 0);
	}

	color = get_bfu_color(term, "dialog.field");
	if (color)
		draw_box(term, &widget_data->box, ' ', 0, color);

	color = get_bfu_color(term, "dialog.field-text");
	if (color) {
		unsigned char *text = widget_data->cdata + widget_data->info.field.vpos;
		int len, w;

#ifdef CONFIG_UTF8
		if (term->utf8_cp && !hide)
			len = utf8_ptr2cells(text, NULL);
		else if (term->utf8_cp)
			len = utf8_ptr2chars(text, NULL);
		else
#endif /* CONFIG_UTF8 */
			len = strlen(text);
		w = int_min(len, widget_data->box.width);

		if (!hide) {
#ifdef CONFIG_UTF8
			if (term->utf8_cp)
				w = utf8_cells2bytes(text, w, NULL);
#endif /* CONFIG_UTF8 */
			draw_text(term, widget_data->box.x, widget_data->box.y,
				  text, w, 0, color);
		} else {
			struct box box;

			copy_box(&box, &widget_data->box);
			box.width = w;

			draw_box(term, &box, '*', 0, color);
		}
	}

	if (sel) {
		int x;

#ifdef CONFIG_UTF8
		if (term->utf8_cp)
			x = widget_data->box.x + len - left;
		else
#endif /* CONFIG_UTF8 */
			x = widget_data->box.x + widget_data->info.field.cpos - widget_data->info.field.vpos;

		set_cursor(term, x, widget_data->box.y, 0);
		set_window_ptr(dlg_data->win, widget_data->box.x, widget_data->box.y);
	}

	return EVENT_PROCESSED;
}

static widget_handler_status_T
display_field(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	return display_field_do(dlg_data, widget_data, 0);
}

static widget_handler_status_T
display_field_pass(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	return display_field_do(dlg_data, widget_data, 1);
}

static widget_handler_status_T
init_field(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	if (widget_has_history(widget_data)) {
		struct input_history_entry *entry;

		foreach (entry, widget_data->widget->info.field.history->entries) {
			int datalen = strlen(entry->data);
			struct input_history_entry *new_entry;

			/* One byte is reserved in struct input_history_entry. */
			new_entry = mem_alloc(sizeof(*new_entry) + datalen);
			if (!new_entry) continue;

			memcpy(new_entry->data, entry->data, datalen + 1);
			add_to_list(widget_data->info.field.history, new_entry);
		}
	}

	widget_data->info.field.cpos = strlen(widget_data->cdata);
	return EVENT_PROCESSED;
}

static int
field_prev_history(struct widget_data *widget_data)
{
	if (widget_has_history(widget_data)
	    && (void *) widget_data->info.field.cur_hist->prev != &widget_data->info.field.history) {
		widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->prev;
		dlg_set_history(widget_data);
		return 1;
	}
	return 0;
}

static int
field_next_history(struct widget_data *widget_data)
{
	if (widget_has_history(widget_data)
	    && (void *) widget_data->info.field.cur_hist != &widget_data->info.field.history) {
		widget_data->info.field.cur_hist = widget_data->info.field.cur_hist->next;
		dlg_set_history(widget_data);
		return 1;
	}
	return 0;
}

static widget_handler_status_T
mouse_field(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct term_event *ev = dlg_data->term_event;

	if (!check_mouse_position(ev, &widget_data->box))
		return EVENT_NOT_PROCESSED;

	/* Handle navigation through history (if any) using up/down mouse wheel */
	switch (get_mouse_button(ev)) {
	case B_WHEEL_UP:
		if (check_mouse_action(ev, B_DOWN)) {
			if (field_prev_history(widget_data)) {
				select_widget(dlg_data, widget_data);
				return EVENT_PROCESSED;
			}
		}
		return EVENT_NOT_PROCESSED;

	case B_WHEEL_DOWN:
		if (check_mouse_action(ev, B_DOWN)) {
			if (field_next_history(widget_data)) {
				select_widget(dlg_data, widget_data);
				return EVENT_PROCESSED;
			}
		}
		return EVENT_NOT_PROCESSED;
	}

	/* Place text cursor at mouse position and focus the widget. */
	widget_data->info.field.cpos = widget_data->info.field.vpos
				     + ev->info.mouse.x - widget_data->box.x;
	int_upper_bound(&widget_data->info.field.cpos, strlen(widget_data->cdata));

	select_widget(dlg_data, widget_data);
	return EVENT_PROCESSED;
}

static widget_handler_status_T
kbd_field(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct window *win = dlg_data->win;
	struct terminal *term = win->term;
	struct term_event *ev = dlg_data->term_event;
	action_id_T action_id;

	action_id = kbd_action(KEYMAP_EDIT, ev, NULL);
	if (action_id != -1
	    && !action_is_anonymous_safe(KEYMAP_EDIT, action_id)
	    && get_cmd_opt_bool("anonymous"))
		return EVENT_NOT_PROCESSED;

	switch (action_id) {
		case ACT_EDIT_UP:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			if (field_prev_history(widget_data)) {
				goto display_field;
			}
			break;

		case ACT_EDIT_DOWN:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			if (field_next_history(widget_data)) {
				goto display_field;
			}
			break;

		case ACT_EDIT_RIGHT:
			if (widget_data->info.field.cpos < strlen(widget_data->cdata)) {
#ifdef CONFIG_UTF8
				if (term->utf8_cp) {
					unsigned char *next = widget_data->cdata + widget_data->info.field.cpos;
					unsigned char *end = strchr(next, '\0');

					utf8_to_unicode(&next, end);
					widget_data->info.field.cpos = (int)(next - widget_data->cdata);
				} else
#endif /* CONFIG_UTF8 */
				{
					widget_data->info.field.cpos++;
				}
			}
			goto display_field;

		case ACT_EDIT_LEFT:
			if (widget_data->info.field.cpos > 0)
				widget_data->info.field.cpos--;
#ifdef CONFIG_UTF8
			if (widget_data->info.field.cpos && term->utf8_cp) {
				unsigned char *t = widget_data->cdata;
				unsigned char *t2 = t;
				int p = widget_data->info.field.cpos;
				unsigned char tmp = t[p];

				t[p] = '\0';
				strlen_utf8(&t2);
				t[p] = tmp;
				widget_data->info.field.cpos = (int)(t2 - t);

			}
#endif /* CONFIG_UTF8 */
			goto display_field;

		case ACT_EDIT_HOME:
			widget_data->info.field.cpos = 0;
			goto display_field;

		case ACT_EDIT_END:
			widget_data->info.field.cpos = strlen(widget_data->cdata);
			goto display_field;

		case ACT_EDIT_BACKSPACE:
#ifdef CONFIG_UTF8
			if (widget_data->info.field.cpos && term->utf8_cp) {
				/* XXX: stolen from src/viewer/text/form.c */
				/* FIXME: This isn't nice. We remove last byte
				 *        from UTF-8 character to detect
				 *        character before it. */
				unsigned char *text = widget_data->cdata;
				unsigned char *end = widget_data->cdata + widget_data->info.field.cpos - 1;
				unicode_val_T data;
				int old = widget_data->info.field.cpos;

				while(1) {
					data = utf8_to_unicode(&text, end);
					if (data == UCS_NO_CHAR)
						break;
				}

				widget_data->info.field.cpos = (int)(text - widget_data->cdata);
				if (old != widget_data->info.field.cpos) {
					int length;

					text = widget_data->cdata;
					length = strlen(text + old) + 1;
					memmove(text + widget_data->info.field.cpos, text + old, length);
				}
				goto display_field;
			}
#endif /* CONFIG_UTF8 */
			if (widget_data->info.field.cpos) {
				memmove(widget_data->cdata + widget_data->info.field.cpos - 1,
					widget_data->cdata + widget_data->info.field.cpos,
					strlen(widget_data->cdata) - widget_data->info.field.cpos + 1);
				widget_data->info.field.cpos--;
			}
			goto display_field;

		case ACT_EDIT_DELETE:
			{
				int cdata_len = strlen(widget_data->cdata);

				if (widget_data->info.field.cpos >= cdata_len) goto display_field;

#ifdef CONFIG_UTF8
				if (term->utf8_cp) {
					unsigned char *end = widget_data->cdata + cdata_len;
					unsigned char *text = widget_data->cdata + widget_data->info.field.cpos;
					unsigned char *old = text;

					utf8_to_unicode(&text, end);
					if (old != text) {
						memmove(old, text,
								(int)(end - text) + 1);
					}
					goto display_field;
				}
#endif /* CONFIG_UTF8 */
				memmove(widget_data->cdata + widget_data->info.field.cpos,
					widget_data->cdata + widget_data->info.field.cpos + 1,
					cdata_len - widget_data->info.field.cpos + 1);
				goto display_field;
			}

		case ACT_EDIT_KILL_TO_BOL:
			memmove(widget_data->cdata,
					widget_data->cdata + widget_data->info.field.cpos,
					strlen(widget_data->cdata + widget_data->info.field.cpos) + 1);
			widget_data->info.field.cpos = 0;
			goto display_field;

		case ACT_EDIT_KILL_TO_EOL:
			widget_data->cdata[widget_data->info.field.cpos] = 0;
			goto display_field;

		case ACT_EDIT_KILL_WORD_BACK:
			{
				int cdata_len = strlen(widget_data->cdata);
				int start = widget_data->info.field.cpos;

				while (start > 0 && isspace(widget_data->cdata[start - 1]))
					--start;
				while (start > 0 && !isspace(widget_data->cdata[start - 1]))
					--start;

				memmove(widget_data->cdata + start,
					widget_data->cdata + widget_data->info.field.cpos,
					cdata_len - widget_data->info.field.cpos + 1);

				widget_data->info.field.cpos = start;

				goto display_field;
			}

		case ACT_EDIT_MOVE_BACKWARD_WORD:
			while (widget_data->info.field.cpos > 0 && isspace(widget_data->cdata[widget_data->info.field.cpos - 1]))
				--widget_data->info.field.cpos;
			while (widget_data->info.field.cpos > 0 && !isspace(widget_data->cdata[widget_data->info.field.cpos - 1]))
				--widget_data->info.field.cpos;

			goto display_field;

		case ACT_EDIT_MOVE_FORWARD_WORD:
			while (isspace(widget_data->cdata[widget_data->info.field.cpos]))
				++widget_data->info.field.cpos;
			while (widget_data->cdata[widget_data->info.field.cpos] && !isspace(widget_data->cdata[widget_data->info.field.cpos]))
				++widget_data->info.field.cpos;
			while (isspace(widget_data->cdata[widget_data->info.field.cpos]))
				++widget_data->info.field.cpos;

			goto display_field;

		case ACT_EDIT_COPY_CLIPBOARD:
			/* Copy to clipboard */
			set_clipboard_text(widget_data->cdata);
			return EVENT_PROCESSED;

		case ACT_EDIT_CUT_CLIPBOARD:
			/* Cut to clipboard */
			set_clipboard_text(widget_data->cdata);
			widget_data->cdata[0] = 0;
			widget_data->info.field.cpos = 0;
			goto display_field;

		case ACT_EDIT_PASTE_CLIPBOARD:
			{
				/* Paste from clipboard */
				unsigned char *clipboard = get_clipboard_text();

				if (!clipboard) goto display_field;

				safe_strncpy(widget_data->cdata, clipboard, widget_data->widget->datalen);
				widget_data->info.field.cpos = strlen(widget_data->cdata);
				mem_free(clipboard);
				goto display_field;
			}

		case ACT_EDIT_AUTO_COMPLETE:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			do_tab_compl(dlg_data, &widget_data->info.field.history);
			goto display_field;

		case ACT_EDIT_AUTO_COMPLETE_FILE:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			do_tab_compl_file(dlg_data, &widget_data->info.field.history);
			goto display_field;

		case ACT_EDIT_AUTO_COMPLETE_UNAMBIGUOUS:
			if (!widget_has_history(widget_data))
				return EVENT_NOT_PROCESSED;

			do_tab_compl_unambiguous(dlg_data, &widget_data->info.field.history);
			goto display_field;

		case ACT_EDIT_REDRAW:
			redraw_terminal_cls(term);
			return EVENT_PROCESSED;

		default:
			if (check_kbd_textinput_key(ev)) {
				unsigned char *text = widget_data->cdata;
				int textlen = strlen(text);
#ifndef CONFIG_UTF8
				/* Both get_kbd_key(ev) and @text
				 * are in the terminal's charset.  */
				const int inslen = 1;
#else  /* CONFIG_UTF8 */
				const unsigned char *ins;
				int inslen;

				/* get_kbd_key(ev) is UCS-4, and @text
				 * is in the terminal's charset.  */
				ins = u2cp_no_nbsp(get_kbd_key(ev),
						   get_terminal_codepage(term));
				inslen = strlen(ins);
#endif /* CONFIG_UTF8 */

				if (textlen >= widget_data->widget->datalen - inslen)
					goto display_field;

				/* Shift to position of the cursor */
				textlen -= widget_data->info.field.cpos;
				text	+= widget_data->info.field.cpos;

				memmove(text + inslen, text, textlen + 1);
#ifdef CONFIG_UTF8
				memcpy(text, ins, inslen);
#else  /* !CONFIG_UTF8 */
				*text = get_kbd_key(ev);
#endif /* !CONFIG_UTF8 */
				widget_data->info.field.cpos += inslen;
				goto display_field;
			}
	}
	return EVENT_NOT_PROCESSED;

display_field:
	display_widget(dlg_data, widget_data);
	redraw_from_window(dlg_data->win);
	return EVENT_PROCESSED;
}


static widget_handler_status_T
clear_field(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	widget_data->info.field.cpos = 0;

	if (widget_data->widget->datalen)
		memset(widget_data->cdata, 0, widget_data->widget->datalen);

	return EVENT_PROCESSED;
}

const struct widget_ops field_ops = {
	display_field,
	init_field,
	mouse_field,
	kbd_field,
	NULL,
	clear_field,
};

const struct widget_ops field_pass_ops = {
	display_field_pass,
	init_field,
	mouse_field,
	kbd_field,
	NULL,
	clear_field,
};


/* Input lines */

static void
input_line_layouter(struct dialog_data *dlg_data)
{
	struct input_line *input_line = dlg_data->dlg->udata;
	struct session *ses = input_line->ses;
	struct window *win = dlg_data->win;
	int y = win->term->height - 1
		- ses->status.show_status_bar
		- ses->status.show_tabs_bar;

	dlg_format_field(win->term, dlg_data->widgets_data, 0,
			 &y, win->term->width, NULL, ALIGN_LEFT, 0);
}

static widget_handler_status_T
input_line_event_handler(struct dialog_data *dlg_data)
{
	struct input_line *input_line = dlg_data->dlg->udata;
	input_line_handler_T handler = input_line->handler;
	enum edit_action action_id;
	struct widget_data *widget_data = dlg_data->widgets_data;
	struct term_event *ev = dlg_data->term_event;

	/* Noodle time */
	switch (ev->ev) {
	case EVENT_KBD:
		action_id = kbd_action(KEYMAP_EDIT, ev, NULL);

		/* Handle some basic actions such as quiting for empty buffers */
		switch (action_id) {
		case ACT_EDIT_ENTER:
		case ACT_EDIT_NEXT_ITEM:
		case ACT_EDIT_PREVIOUS_ITEM:
			if (widget_has_history(widget_data))
				add_to_input_history(widget_data->widget->info.field.history,
						     input_line->buffer, 1);
			break;

		case ACT_EDIT_BACKSPACE:
			if (!*input_line->buffer)
				goto cancel_input_line;
			break;

		case ACT_EDIT_CANCEL:
			goto cancel_input_line;

		default:
			break;
		}

		/* First let the input field do its business */
		kbd_field(dlg_data, widget_data);
		break;

	case EVENT_MOUSE:
#ifdef CONFIG_MOUSE
		if (ev->info.mouse.y != dlg_data->win->y) {
			delete_window_ev(dlg_data->win, ev);
			return EVENT_PROCESSED;
		}
#endif /* CONFIG_MOUSE */
		return EVENT_NOT_PROCESSED;

	case EVENT_REDRAW:
		/* Try to catch the redraw event initiated by the history
		 * completion and only respond if something was actually
		 * updated. Meaning we have new data in the line buffer that
		 * should be propagated to the line handler. */
		if (!widget_has_history(widget_data)
		    || widget_data->info.field.cpos <= 0
		    || widget_data->info.field.cpos <= strlen(input_line->buffer))
			return EVENT_NOT_PROCESSED;

		/* Fall thru */

	case EVENT_RESIZE:
		action_id = ACT_EDIT_REDRAW;
		break;

	default:
		return EVENT_NOT_PROCESSED;
	}

	update_dialog_data(dlg_data);

send_action_to_handler:
	/* Then pass it on to the specialized handler */
	switch (handler(input_line, action_id)) {
	case INPUT_LINE_CANCEL:
cancel_input_line:
		cancel_dialog(dlg_data, widget_data);
		break;

	case INPUT_LINE_REWIND:
		/* This is stolen kbd_field() handling for ACT_EDIT_BACKSPACE */
		memmove(widget_data->cdata + widget_data->info.field.cpos - 1,
			widget_data->cdata + widget_data->info.field.cpos,
			strlen(widget_data->cdata) - widget_data->info.field.cpos + 1);
		widget_data->info.field.cpos--;

		update_dialog_data(dlg_data);

		goto send_action_to_handler;

	case INPUT_LINE_PROCEED:
		break;
	}

	/* Hack: We want our caller to perform its redrawing routine,
	 * even if we did process the event here. */
	if (action_id == ACT_EDIT_REDRAW) return EVENT_NOT_PROCESSED;

	/* Completely bypass any further dialog event handling */
	return EVENT_PROCESSED;
}

void
input_field_line(struct session *ses, unsigned char *prompt, void *data,
		 struct input_history *history, input_line_handler_T handler)
{
	struct dialog *dlg;
	unsigned char *buffer;
	struct input_line *input_line;

	assert(ses);

	dlg = calloc_dialog(INPUT_LINE_WIDGETS, sizeof(*input_line));
	if (!dlg) return;

	input_line = (void *) get_dialog_offset(dlg, INPUT_LINE_WIDGETS);
	input_line->ses = ses;
	input_line->data = data;
	input_line->handler = handler;
	buffer = input_line->buffer;

	dlg->handle_event = input_line_event_handler;
	dlg->layouter = input_line_layouter;
	dlg->layout.only_widgets = 1;
	dlg->udata = input_line;

	add_dlg_field_float2(dlg, prompt, 0, 0, NULL, INPUT_LINE_BUFFER_SIZE,
			     buffer, history);

	do_dialog(ses->tab->term, dlg, getml(dlg, (void *) NULL));
}
