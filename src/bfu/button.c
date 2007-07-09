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
add_dlg_button_do(const unsigned char *file, int line,
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

#ifdef CONFIG_UTF8
static void
buttons_width(struct widget_data *widget_data, int n,
	      int *minwidth, int *maxwidth, int utf8)
#else
static void
buttons_width(struct widget_data *widget_data, int n,
	      int *minwidth, int *maxwidth)
#endif /* CONFIG_UTF8 */
{
	int maxw = -BUTTON_HSPACING;
#ifdef CONFIG_UTF8
	int button_lr_len = utf8_ptr2cells(BUTTON_LEFT, NULL)
			  + utf8_ptr2cells(BUTTON_RIGHT, NULL);
#endif /* CONFIG_UTF8 */

	assert(n > 0);
	if_assert_failed return;

	while (n--) {
		int minw;
#ifdef CONFIG_UTF8
		if (utf8)
			minw = utf8_ptr2cells((widget_data++)->widget->text, NULL)
			       + BUTTON_HSPACING + button_lr_len;
		else
#endif /* CONFIG_UTF8 */
			minw = (widget_data++)->widget->info.button.textlen
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
#ifdef CONFIG_UTF8
			buttons_width(widget_data1, i2 - i1 + 1, NULL, &mw,
				      term->utf8_cp);
#else
			buttons_width(widget_data1, i2 - i1 + 1, NULL, &mw);
#endif /* CONFIG_UTF8 */
			if (mw <= w) i2++;
			else break;
		}

		mw = 0;
#ifdef CONFIG_UTF8
		buttons_width(widget_data1, i2 - i1, NULL, &mw, term->utf8_cp);
#else
		buttons_width(widget_data1, i2 - i1, NULL, &mw);
#endif /* CONFIG_UTF8 */
		if (rw) int_bounds(rw, mw, w);

		if (!format_only) {
			int i;
			int p = x + (align == ALIGN_CENTER ? (w - mw) / 2 : 0);
#ifdef CONFIG_UTF8
			int button_lr_len = utf8_ptr2cells(BUTTON_LEFT, NULL)
					  + utf8_ptr2cells(BUTTON_RIGHT, NULL);
#endif /* CONFIG_UTF8 */

			for (i = i1; i < i2; i++) {
#ifdef CONFIG_UTF8
				if (term->utf8_cp)
					set_box(&widget_data[i].box,
						p, *y,
						utf8_ptr2cells(widget_data[i].widget->text, NULL)
						+ button_lr_len, BUTTON_HEIGHT);
				else
#endif /* CONFIG_UTF8 */
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
	int len, x;
	int sel = is_selected_widget(dlg_data, widget_data);

	if (sel) {
		shortcut_color = get_bfu_color(term, "dialog.button-shortcut-selected");
		color =  get_bfu_color(term, "dialog.button-selected");
	} else {
		shortcut_color = get_bfu_color(term, "dialog.button-shortcut");
		color =  get_bfu_color(term, "dialog.button");
	}
	if (!color || !shortcut_color) return EVENT_PROCESSED;

#ifdef CONFIG_UTF8
	if (term->utf8_cp) {
		int button_left_len = utf8_ptr2cells(BUTTON_LEFT, NULL);
		int button_right_len = utf8_ptr2cells(BUTTON_RIGHT, NULL);

		x = pos->x + button_left_len;
		len = widget_data->box.width -
			(button_left_len + button_right_len);

	} else
#endif /* CONFIG_UTF8 */
	{
		x = pos->x + BUTTON_LEFT_LEN;
		len = widget_data->box.width - BUTTON_LR_LEN;
	}


	draw_text(term, pos->x, pos->y, BUTTON_LEFT, BUTTON_LEFT_LEN, 0, color);
	if (len > 0) {
		unsigned char *text = widget_data->widget->text;
		int hk_pos = widget_data->widget->info.button.hotkey_pos;
		int attr;

		attr = get_opt_bool("ui.dialogs.underline_button_shortcuts")
		     ? SCREEN_ATTR_UNDERLINE : 0;

#ifdef CONFIG_UTF8
		if (term->utf8_cp) {
			if (hk_pos >= 0) {
				int hk_bytes = utf8charlen(&text[hk_pos+1]);
				int cells_to_hk = utf8_ptr2cells(text,
						&text[hk_pos]);
				int right = widget_data->widget->info.button.truetextlen
					- hk_pos
					- hk_bytes;

				int hk_cells = utf8_char2cells(&text[hk_pos
								      + 1],
								NULL);

				if (hk_pos)
					draw_text(term, x, pos->y,
						  text, hk_pos, 0, color);

				draw_text(term, x + cells_to_hk, pos->y,
					  &text[hk_pos + 1], hk_bytes,
					  attr, shortcut_color);

				if (right > 1)
					draw_text(term, x+cells_to_hk+hk_cells,
						  pos->y,
						  &text[hk_pos + hk_bytes + 1],
						  right - 1, 0, color);

			} else {
				int hk_width = utf8_char2cells(text, NULL);
				int hk_len = utf8charlen(text);
				int len_to_display =
					utf8_cells2bytes(&text[hk_len],
							 len - hk_width,
							 NULL);

				draw_text(term, x, pos->y,
					  text, hk_len,
					  attr, shortcut_color);

				draw_text(term, x + hk_width, pos->y,
					  &text[hk_len], len_to_display,
					  0, color);
			}
		} else
#endif /* CONFIG_UTF8 */
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
#ifdef CONFIG_UTF8
	if (term->utf8_cp) {
		int text_cells = utf8_ptr2cells(widget_data->widget->text, NULL);
		int hk = (widget_data->widget->info.button.hotkey_pos >= 0);

		draw_text(term, x + text_cells - hk, pos->y,
			  BUTTON_RIGHT, BUTTON_RIGHT_LEN, 0, color);
	} else
#endif /* CONFIG_UTF8 */
		draw_text(term, x + len, pos->y, BUTTON_RIGHT,
			  BUTTON_RIGHT_LEN, 0, color);
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

const struct widget_ops button_ops = {
	display_button,
	NULL,
	mouse_button,
	NULL,
	select_button,
	NULL,
};
