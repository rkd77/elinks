#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#ifdef __cplusplus
extern "C" {
#endif

struct dialog;
struct dialog_data;
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


void add_dlg_radio_do(struct dialog *dlg, char *text, int groupid, int groupnum, int *data);

#define add_dlg_radio(dlg, text, groupid, groupnum, data) \
	add_dlg_radio_do(dlg, text, groupid, groupnum, data)

#define add_dlg_checkbox(dlg, text, data) \
	add_dlg_radio_do(dlg, text, 0, 0, data)

extern const struct widget_ops checkbox_ops;

void
dlg_format_checkbox(struct dialog_data *dlg_data,
		    struct widget_data *widget_data,
		    int x, int *y, int w, int *rw,
		    format_align_T align, int format_only);

#define widget_has_group(widget_data)	((widget_data)->widget->type == WIDGET_CHECKBOX \
					  ? (widget_data)->widget->info.checkbox.gid : -1)

#ifdef __cplusplus
}
#endif

#endif
