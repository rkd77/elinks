/* Widget group implementation. */
/* $Id: group.c,v 1.72 2005/03/24 11:44:42 zas Exp $ */

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

void
dlg_format_group(struct terminal *term,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw)
{
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
		int label_length = (text && *text) ? strlen(text) : 0;
		int label_padding = (label_length > 0);

		if (widget_data->widget->type == WIDGET_CHECKBOX) {
			width = 3;
		} else if (widget_is_textfield(widget_data)) {
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

		if (term) {
			if (widget_data->widget->type == WIDGET_CHECKBOX) {
				/* Draw text at right of checkbox. */
				if (label_length)
					draw_text(term, xpos + width + label_padding, *y,
						  text, label_length,
						  0, color);

				set_box(&widget_data->box, xpos, *y, width, 1);

			} else if (widget_is_textfield(widget_data)) {
				/* Draw label at left of widget. */
				if (label_length)
					draw_text(term, xpos, *y,
						  text, label_length,
						  0, color);

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
	int rw = int_min(w, strlen(dlg_data->dlg->title));
	int y = 0;
	int n = dlg_data->number_of_widgets - 2;

	dlg_format_group(NULL, dlg_data->widgets_data, n,
			 0, &y, w, &rw);

	y++;
	dlg_format_buttons(NULL, dlg_data->widgets_data + n, 2, 0, &y, w,
			   &rw, ALIGN_CENTER);

	w = rw;

	draw_dialog(dlg_data, w, y);

	y = dlg_data->box.y + DIALOG_TB + 1;
	dlg_format_group(term, dlg_data->widgets_data, n,
			 dlg_data->box.x + DIALOG_LB, &y, w, NULL);

	y++;
	dlg_format_buttons(term, dlg_data->widgets_data + n, 2,
			   dlg_data->box.x + DIALOG_LB, &y, w, &rw, ALIGN_CENTER);
}
