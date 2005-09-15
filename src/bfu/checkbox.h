/* $Id: checkbox.h,v 1.38 2004/11/19 19:05:40 jonas Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

struct dialog;
struct terminal;
struct widget_data;

struct widget_info_checkbox {
	/* gid is 0 for checkboxes, or a positive int for
	 * each group of radio buttons. */
	int gid;
	/* gnum is 0 for checkboxes, or a positive int for
	 * each radio button in a group. */
	int gnum;
};

struct widget_data_info_checkbox {
	int checked;
};


void add_dlg_radio_do(struct dialog *dlg, unsigned char *text, int groupid, int groupnum, void *data);

#define add_dlg_radio(dlg, text_, groupid, groupnum, data_) \
	add_dlg_radio_do(dlg, text_, groupid, groupnum, (void *) &(data_))

#define add_dlg_checkbox(dlg, text_, data_) \
	add_dlg_radio_do(dlg, text_, 0, 0, (void *) &(data_))

extern struct widget_ops checkbox_ops;

void
dlg_format_checkbox(struct terminal *term,
		    struct widget_data *widget_data,
		    int x, int *y, int w, int *rw,
		    enum format_align align);

#define widget_has_group(widget_data)	((widget_data)->widget->type == WIDGET_CHECKBOX \
					  ? (widget_data)->widget->info.checkbox.gid : -1)

#endif
