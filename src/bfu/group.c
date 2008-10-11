/* Widget group implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/group.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/color.h"

/* Same as in src/bfu/checkbox.c */
#define CHECKBOX_LEN 3  /* "[X]" or "(X)" */

void
dlg_format_group(struct dialog_data *dlg_data,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw, int format_only)
{
	struct terminal *term = dlg_data->win->term;
	int space_between_widgets = 1;
	int line_width = 0;
	int xpos;
	struct color_pair *color = get_bfu_color(term, "dialog.text");

	assert(n > 0);
	if_assert_failed return;

	while (n--) {
		int widget_width;
		int width;
		unsigned char *text = widget_data->widget->text;
		int label_length;
		int label_padding;

#ifdef CONFIG_UTF8
		if (term->utf8_cp) {
			if (text && *text)
				label_length = utf8_ptr2cells(text, NULL);
			else
				label_length = 0;
		} else
#endif /* CONFIG_UTF8 */
			label_length = (text && *text) ? strlen(text) : 0;

		label_padding = (label_length > 0);

		if (widget_data->widget->type == WIDGET_CHECKBOX) {
			width = CHECKBOX_LEN;
		} else if (widget_is_textfield(widget_data)) {
#ifdef CONFIG_UTF8
			if (term->utf8_cp) {
				width = utf8_ptr2cells(widget_data->widget->data,
						       NULL);
			} else
#endif /* CONFIG_UTF8 */
				width = widget_data->widget->datalen;
		} else {
			/* TODO: handle all widget types. */
			widget_data++;
			continue;
		}

		int_bounds(&label_length, 0, w - width - label_padding);

		widget_width = width + label_padding + label_length;
		if (line_width + widget_width > w) {
			line_width = 0;
			(*y) += 2;	/* Next line */
		}

		xpos = x + line_width;

		if (!format_only) {
			if (widget_data->widget->type == WIDGET_CHECKBOX) {
				/* Draw text at right of checkbox. */
				if (label_length) {
#ifdef CONFIG_UTF8
					if (term->utf8_cp) {
						int lb = utf8_cells2bytes(
								text,
								label_length,
								NULL);
						draw_dlg_text(dlg_data, xpos + width
								+ label_padding,
							  *y, text, lb, 0,
							  color);
					} else
#endif /* CONFIG_UTF8 */
					{
						draw_dlg_text(dlg_data, xpos + width
								+ label_padding,
							  *y, text,
							  label_length, 0,
							  color);
					}
				}

				set_box(&widget_data->box, xpos, *y, width, 1);

			} else if (widget_is_textfield(widget_data)) {
				/* Draw label at left of widget. */
				if (label_length) {
#ifdef CONFIG_UTF8
					if (term->utf8_cp) {
						int lb = utf8_cells2bytes(
								text,
								label_length,
								NULL);
						draw_dlg_text(dlg_data, xpos, *y,
							  text, lb, 0, color);
					} else
#endif /* CONFIG_UTF8 */
					{
						draw_dlg_text(dlg_data, xpos, *y,
							  text, label_length,
							  0, color);
					}
				}

				set_box(&widget_data->box,
					xpos + label_padding + label_length, *y,
					width, 1);
			}
		}

		line_width += widget_width;
		if (rw) int_bounds(rw, line_width, w);
		line_width += space_between_widgets;

		widget_data++;
	}
	(*y)++;
}

void
group_layouter(struct dialog_data *dlg_data)
{
	struct terminal *term = dlg_data->win->term;
	int w = dialog_max_width(term);
	int rw;
	int y = 0;
	int n = dlg_data->number_of_widgets - 2;

#ifdef CONFIG_UTF8
	if (term->utf8_cp)
		rw = int_min(w, utf8_ptr2cells(dlg_data->dlg->title, NULL));
	else
#endif /* CONFIG_UTF8 */
		rw = int_min(w, strlen(dlg_data->dlg->title));

	dlg_format_group(dlg_data, dlg_data->widgets_data, n,
			 0, &y, w, &rw, 1);

	y++;
	dlg_format_buttons(dlg_data, dlg_data->widgets_data + n, 2, 0, &y, w,
			   &rw, ALIGN_CENTER, 1);

	w = rw;

	draw_dialog(dlg_data, w, y);

	y = dlg_data->box.y + DIALOG_TB + 1;
	dlg_format_group(dlg_data, dlg_data->widgets_data, n,
			 dlg_data->box.x + DIALOG_LB, &y, w, NULL, 0);

	y++;
	dlg_format_buttons(dlg_data, dlg_data->widgets_data + n, 2,
			   dlg_data->box.x + DIALOG_LB, &y, w, &rw, ALIGN_CENTER, 0);
}
