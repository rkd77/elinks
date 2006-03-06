/* Button widget handlers. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"
#include "util/align.h"

/* Height of a button. */
#define BUTTON_HEIGHT 1

/* Vertical spacing between buttons. */
#define BUTTON_VSPACING	1

/* Horizontal spacing between buttons. */
#define BUTTON_HSPACING	2

/* Left and right text appearing around label of button.
 * Currently a dialog button is displayed as [ LABEL ] */
#define BUTTON_LEFT "[ "
#define BUTTON_RIGHT " ]"
#define BUTTON_LEFT_LEN (sizeof(BUTTON_LEFT) - 1)
#define BUTTON_RIGHT_LEN (sizeof(BUTTON_RIGHT) - 1)

#define BUTTON_LR_LEN (BUTTON_LEFT_LEN + BUTTON_RIGHT_LEN)

#ifdef DEBUG_BUTTON_HOTKEY
void
add_dlg_button_do(unsigned char *file, int line,
		  struct dialog *dlg, unsigned char *text, int flags,
		  widget_handler_T *handler, void *data,
		  done_handler_T *done, void *done_data)
#else
void
add_dlg_button_do(struct dialog *dlg, unsigned char *text, int flags,
		  widget_handler_T *handler, void *data,
		  done_handler_T *done, void *done_data)
#endif
{
	int textlen = strlen(text);
	struct widget *widget = &dlg->widgets[dlg->number_of_widgets++];

	widget->type	= WIDGET_BUTTON;
	widget->handler	= handler;
	widget->text	= text;
	widget->data	= data;

	widget->info.button.flags       = flags;
	widget->info.button.done        = done;
	widget->info.button.done_data   = done_data;
	widget->info.button.hotkey_pos  = -1;
	widget->info.button.textlen     = textlen;
	widget->info.button.truetextlen = textlen;

	if (textlen > 1) {
		unsigned char *pos = memchr(text, '~', textlen - 1);

		if (pos) {
			widget->info.button.hotkey_pos = pos - text;
			widget->info.button.textlen--;
		}
#ifdef DEBUG_BUTTON_HOTKEY
		else {
			DBG("%s:%d missing keyboard accelerator in \"%s\".", file, line, text);
		}
#endif
	}
}

static void
buttons_width(struct widget_data *widget_data, int n,
	      int *minwidth, int *maxwidth)
{
	int maxw = -BUTTON_HSPACING;

	assert(n > 0);
	if_assert_failed return;

	while (n--) {
		int minw = (widget_data++)->widget->info.button.textlen
				+ BUTTON_HSPACING + BUTTON_LR_LEN;

		maxw += minw;
		if (minwidth) int_lower_bound(minwidth, minw);
	}

	if (maxwidth) int_lower_bound(maxwidth, maxw);
}

void
dlg_format_buttons(struct terminal *term,
		   struct widget_data *widget_data, int n,
		   int x, int *y, int w, int *rw, enum format_align align, int format_only)
{
	int i1 = 0;

	while (i1 < n) {
		struct widget_data *widget_data1 = widget_data + i1;
		int i2 = i1 + 1;
		int mw;

		while (i2 < n) {
			mw = 0;
			buttons_width(widget_data1, i2 - i1 + 1, NULL, &mw);
			if (mw <= w) i2++;
			else break;
		}

		mw = 0;
		buttons_width(widget_data1, i2 - i1, NULL, &mw);
		if (rw) int_bounds(rw, mw, w);

		if (!format_only) {
			int i;
			int p = x + (align == ALIGN_CENTER ? (w - mw) / 2 : 0);

			for (i = i1; i < i2; i++) {
				set_box(&widget_data[i].box,
					 p, *y,
					 widget_data[i].widget->info.button.textlen
					 + BUTTON_LR_LEN, BUTTON_HEIGHT);

				p += widget_data[i].box.width + BUTTON_HSPACING;
			}
		}

		*y += BUTTON_VSPACING + BUTTON_HEIGHT;
		i1 = i2;
	}
}

static widget_handler_status_T
display_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color, *shortcut_color;
	struct box *pos = &widget_data->box;
	int len = widget_data->box.width - BUTTON_LR_LEN;
	int x = pos->x + BUTTON_LEFT_LEN;
	int sel = is_selected_widget(dlg_data, widget_data);

	if (sel) {
		shortcut_color = get_bfu_color(term, "dialog.button-shortcut-selected");
		color =  get_bfu_color(term, "dialog.button-selected");
	} else {
		shortcut_color = get_bfu_color(term, "dialog.button-shortcut");
		color =  get_bfu_color(term, "dialog.button");
	}
	if (!color || !shortcut_color) return EVENT_PROCESSED;

	draw_text(term, pos->x, pos->y, BUTTON_LEFT, BUTTON_LEFT_LEN, 0, color);
	if (len > 0) {
		unsigned char *text = widget_data->widget->text;
		int hk_pos = widget_data->widget->info.button.hotkey_pos;
		int attr;

		attr = get_opt_bool("ui.dialogs.underline_button_shortcuts")
		     ? SCREEN_ATTR_UNDERLINE : 0;

#ifdef CONFIG_UTF_8
		if (term->utf8) {
			unsigned char *text2 = text;
			unsigned char *end = text
				+ widget_data->widget->info.button.truetextlen;
			int hk_state = 0;
			int x1;

			for (x1 = 0; x1 - !!hk_state < len && *text2; x1++) {
				uint16_t data;

				data = (uint16_t)utf_8_to_unicode(&text2, end);
				if (!hk_state && (int)(text2 - text) == hk_pos + 1) {
					hk_state = 1;
					continue;
				}
				if (hk_state == 1) {
					draw_char(term, x + x1 - 1, pos->y, data, attr, shortcut_color);
					hk_state = 2;
				} else {
					draw_char(term, x + x1 - !!hk_state, pos->y, data, 0, color);
				}


			}
			len = x1 - !!hk_state;
		} else
#endif /* CONFIG_UTF_8 */
		if (hk_pos >= 0) {
			int right = widget_data->widget->info.button.truetextlen - hk_pos - 1;

			if (hk_pos) {
				draw_text(term, x, pos->y, text, hk_pos, 0, color);
			}
			draw_text(term, x + hk_pos, pos->y,
				  &text[hk_pos + 1], 1, attr, shortcut_color);
			if (right > 1) {
				draw_text(term, x + hk_pos + 1, pos->y,
					  &text[hk_pos + 2], right - 1, 0, color);
			}

		} else {
			draw_text(term, x, pos->y, text, 1, attr, shortcut_color);
			draw_text(term, x + 1, pos->y, &text[1], len - 1, 0, color);
		}
	}
	draw_text(term, x + len, pos->y, BUTTON_RIGHT, BUTTON_RIGHT_LEN, 0, color);

	if (sel) {
		set_cursor(term, x, pos->y, 1);
		set_window_ptr(dlg_data->win, pos->x, pos->y);
	}
	return EVENT_PROCESSED;
}

static widget_handler_status_T
mouse_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct terminal *term = dlg_data->win->term;
	struct term_event *ev = dlg_data->term_event;

	if (check_mouse_wheel(ev))
		return EVENT_NOT_PROCESSED;

	if (!check_mouse_position(ev, &widget_data->box))
		return EVENT_NOT_PROCESSED;

	select_widget(dlg_data, widget_data);

	do_not_ignore_next_mouse_event(term);

	if (check_mouse_action(ev, B_UP) && widget_data->widget->ops->select)
		return widget_data->widget->ops->select(dlg_data, widget_data);

	return EVENT_PROCESSED;
}

static widget_handler_status_T
select_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	return widget_data->widget->handler(dlg_data, widget_data);
}

struct widget_ops button_ops = {
	display_button,
	NULL,
	mouse_button,
	NULL,
	select_button,
	NULL,
};
