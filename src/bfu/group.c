/* Widget group implementation. */
/* $Id: group.c,v 1.64.4.1 2005/01/04 00:05:48 jonas Exp $ */

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

static inline int
base_group_width(struct widget_data *widget_data)
{
	if (widget_data->widget->type == WIDGET_CHECKBOX)
		return 4;

	if (widget_data->widget->type == WIDGET_BUTTON)
		return strlen(widget_data->widget->text) + 5;

	return widget_data->widget->datalen + 1;
}

void
dlg_format_group(struct terminal *term,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw)
{
	int nx = 0;
	int base = base_group_width(widget_data);
	struct color_pair *color = get_bfu_color(term, "dialog.text");

	assert(n > 0);
	if_assert_failed return;

	while (n--) {
		int sl;
		int wx = base;
		unsigned char *text = empty_string_or_(widget_data->widget->text);

		if (text[0]) {
			sl = strlen(text);
		} else {
			sl = -1;
		}

		wx += sl;
		if (nx && nx + wx > w) {
			nx = 0;
			(*y) += 2;
		}

		if (term) {
			int is_checkbox = (widget_data->widget->type == WIDGET_CHECKBOX);
			int xnx = x + nx;
			int width = 1;

			draw_text(term, xnx + 4 * is_checkbox, *y,
				  text, ((sl == -1) ? strlen(text) : sl),
				  0, color);

			if (is_checkbox)
				width = 4;
			else if (widget_is_textfield(widget_data))
				width = widget_data->widget->datalen;

			set_box(&widget_data->box,
				xnx + !is_checkbox * (sl + 1), *y,
				width, 1);
		}

		if (rw) int_bounds(rw, nx + wx, w);

		nx += wx + 1;
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
