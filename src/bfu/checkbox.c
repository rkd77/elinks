/* Checkbox widget handlers. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/dialog.h"
#include "bfu/text.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"

#define CHECKBOX_HEIGHT 1
#define CHECKBOX_LEN 3	/* "[X]" or "(X)" */
#define CHECKBOX_SPACING 1	/* "[X]" + " " + "Label" */
#define CHECKBOX_LS (CHECKBOX_LEN + CHECKBOX_SPACING)	/* "[X] " */

void
add_dlg_radio_do(struct dialog *dlg, unsigned char *text,
		 int groupid, int groupnum, int *data)
{
	struct widget *widget = &dlg->widgets[dlg->number_of_widgets++];

	widget->type    = WIDGET_CHECKBOX;
	widget->text    = text;
	widget->datalen = sizeof(*data);
	widget->data    = (void *) data;

	widget->info.checkbox.gid  = groupid;
	widget->info.checkbox.gnum = groupnum;
}

void
dlg_format_checkbox(struct terminal *term,
		    struct widget_data *widget_data,
		    int x, int *y, int w, int *rw,
		    enum format_align align, int format_only)
{
	unsigned char *text = widget_data->widget->text;

	set_box(&widget_data->box, x, *y, CHECKBOX_LEN, CHECKBOX_HEIGHT);

	if (w <= CHECKBOX_LS) return;

	if (text && *text) {
		if (rw) *rw -= CHECKBOX_LS;
		dlg_format_text_do(term, text, x + CHECKBOX_LS, y,
				   w - CHECKBOX_LS, rw,
				   get_bfu_color(term, "dialog.checkbox-label"),
				   align, format_only);
		if (rw) *rw += CHECKBOX_LS;
	}
}

static widget_handler_status_T
display_checkbox(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct terminal *term = dlg_data->win->term;
	struct color_pair *color;
	unsigned char *text;
	struct box *pos = &widget_data->box;
	int selected = is_selected_widget(dlg_data, widget_data);

	if (selected) {
		color = get_bfu_color(term, "dialog.checkbox-selected");
	} else {
		color = get_bfu_color(term, "dialog.checkbox");
	}
	if (!color) return EVENT_PROCESSED;

	if (widget_data->info.checkbox.checked)
		text = widget_data->widget->info.checkbox.gid ? "(X)" : "[X]";
	else
		text = widget_data->widget->info.checkbox.gid ? "( )" : "[ ]";

	draw_text(term, pos->x, pos->y, text, CHECKBOX_LEN, 0, color);

	if (selected) {
		set_cursor(term, pos->x + 1, pos->y, 1);
		set_window_ptr(dlg_data->win, pos->x, pos->y);
	}

	return EVENT_PROCESSED;
}

static widget_handler_status_T
init_checkbox(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	int *cdata = (int *) widget_data->cdata;

	assert(cdata);

	if (widget_data->widget->info.checkbox.gid) {
		if (*cdata == widget_data->widget->info.checkbox.gnum)
			widget_data->info.checkbox.checked = 1;
	} else {
		if (*cdata)
			widget_data->info.checkbox.checked = 1;
	}
	return EVENT_PROCESSED;
}

static widget_handler_status_T
mouse_checkbox(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct terminal *term = dlg_data->win->term;
	struct term_event *ev = dlg_data->term_event;

	if (check_mouse_wheel(ev)
	    || !check_mouse_position(ev, &widget_data->box))
		return EVENT_NOT_PROCESSED;

	select_widget(dlg_data, widget_data);

	do_not_ignore_next_mouse_event(term);

	if (check_mouse_action(ev, B_UP) && widget_data->widget->ops->select)
		widget_data->widget->ops->select(dlg_data, widget_data);

	return EVENT_PROCESSED;
}

static widget_handler_status_T
select_checkbox(struct dialog_data *dlg_data, struct widget_data *widget_data)
{

	if (!widget_data->widget->info.checkbox.gid) {
		/* Checkbox. */
		int *cdata = (int *) widget_data->cdata;

		assert(cdata);

		widget_data->info.checkbox.checked = *cdata = !*cdata;

	} else {
		/* Group of radio buttons. */
		struct widget_data *wdata;

		foreach_widget(dlg_data, wdata) {
			int *cdata = (int *) wdata->cdata;

			if (wdata->widget->type != WIDGET_CHECKBOX)
				continue;

			if (wdata->widget->info.checkbox.gid
			    != widget_data->widget->info.checkbox.gid)
				continue;

			assert(cdata);

			*cdata = widget_data->widget->info.checkbox.gnum;
			wdata->info.checkbox.checked = 0;
			display_widget(dlg_data, wdata);
		}
		widget_data->info.checkbox.checked = 1;
	}

	display_widget(dlg_data, widget_data);
	return EVENT_PROCESSED;
}

const struct widget_ops checkbox_ops = {
	display_checkbox,
	init_checkbox,
	mouse_checkbox,
	NULL,
	select_checkbox,
	NULL,
};
