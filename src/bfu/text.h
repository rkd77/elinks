#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "util/color.h"

struct dialog;
struct terminal;

struct widget_info_text	{
	enum format_align align;
	unsigned int is_label:1;
	unsigned int is_scrollable:1;
};

struct widget_data_info_text {
	/* The number of the first line that should be
	 * displayed within the widget.
	 * This is used only for scrollable text widgets */
	int current;

	/* The number of lines saved in @cdata */
	int lines;

	/* The dialog width to which the lines are wrapped.
	 * This is used to check whether the lines must be
	 * rewrapped. */
	int max_width;
#ifdef CONFIG_MOUSE
	/* For mouse scrollbar handling. See bfu/text.c.*/

	/* Height of selected part of scrollbar. */
	int scroller_height;

	/* Position of selected part of scrollbar. */
	int scroller_y;

	/* Direction of last mouse scroll. Used to adjust
	 * scrolling when selected bar part has a low height
	 * (especially the 1 char height) */
	int scroller_last_dir;
#endif
};

void add_dlg_text(struct dialog *dlg, unsigned char *text,
		  enum format_align align, int bottom_pad);

extern const struct widget_ops text_ops;
void dlg_format_text_do(struct terminal *term,
		    unsigned char *text, int x, int *y, int w, int *rw,
		    struct color_pair *scolor, enum format_align align, int format_only);

void
dlg_format_text(struct terminal *term, struct widget_data *widget_data,
		int x, int *y, int dlg_width, int *real_width, int height, int format_only);

#define text_is_scrollable(widget_data) \
	((widget_data)->widget->info.text.is_scrollable \
	 && (widget_data)->box.height > 0 \
	 && (widget_data)->info.text.lines > 0 \
	 && (widget_data)->box.height < (widget_data)->info.text.lines)

#endif
