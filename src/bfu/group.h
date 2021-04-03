#ifndef EL__BFU_GROUP_H
#define EL__BFU_GROUP_H

#ifdef __cplusplus
extern "C" {
#endif

struct dialog_data;
struct terminal;
struct widget_data;

void dlg_format_group(struct dialog_data *dlg_data,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw, int format_only);

void group_layouter(struct dialog_data *);

#ifdef __cplusplus
}
#endif

#endif
