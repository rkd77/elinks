#ifndef EL__BFU_WIDGET_H
#define EL__BFU_WIDGET_H

/* XXX: Include common definitions first */
#include "bfu/common.h"

#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/group.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/leds.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "util/lists.h"
#include "util/box.h"

struct dialog_data;


struct widget_ops {
	/* XXX: Order matters here. --Zas */
	widget_handler_T *display;
	widget_handler_T *init;
	widget_handler_T *mouse;
	widget_handler_T *kbd;
	widget_handler_T *select;
	widget_handler_T *clear;
};

struct widget {
	const struct widget_ops *ops;

	unsigned char *text;

	widget_handler_T *handler;

	void *data;
	int datalen;	/* 0 = no alloc/copy to cdata. */

	union {
		struct widget_info_checkbox checkbox;
		struct widget_info_field field;
		struct widget_info_button button;
		struct widget_info_text text;
	} info;

	enum widget_type type;
};

struct widget_data {
	struct widget *widget;

	/* For WIDGET_FIELD: @cdata is in the charset of the terminal.
	 * (That charset can be UTF-8 only if CONFIG_UTF8 is defined,
	 * and is assumed to be unibyte otherwise.)  The UTF-8 I/O
	 * option has no effect here.
	 *
	 * For WIDGET_TEXT: @cdata is cast from/to an unsigned char **
	 * that points to the first element of an array.  Each element
	 * in this array corresponds to one line of text, and is an
	 * unsigned char * that points to the first character of that
	 * line.  The array has @widget_data.info.text.lines elements.
	 *
	 * For WIDGET_LISTBOX: @cdata points to struct listbox_data.  */
	unsigned char *cdata;

	struct box box;

	union {
		struct widget_data_info_field field;
		struct widget_data_info_checkbox checkbox;
		struct widget_data_info_text text;
	} info;
};

void display_widget(struct dialog_data *, struct widget_data *);

static inline int
widget_is_focusable(struct widget_data *widget_data)
{
	switch (widget_data->widget->type) {
		case WIDGET_LISTBOX: return 0;
		case WIDGET_TEXT: return text_is_scrollable(widget_data);
		default: return 1;
	}
}


#endif
