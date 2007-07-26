#ifndef EL__BFU_INPFIELD_H
#define EL__BFU_INPFIELD_H

#include "bfu/common.h"
#include "util/memlist.h"
#include "util/lists.h"
#include "util/align.h"


struct dialog;
struct dialog_data;
struct input_history;
struct session;
struct terminal;
struct widget_data;

enum inpfield_flags {
	INPFIELD_NONE = 0,	/* Field label on first line, value on next one. */
	INPFIELD_FLOAT = 1,	/* Field label followed by ':' and value on the same line. */
	INPFIELD_FLOAT2 = 2,	/* Field label followed by value on the same line. */
};

struct widget_info_field {
	int min;
	int max;
	struct input_history *history;
	enum inpfield_flags flags;
};

struct widget_data_info_field {
	int vpos;
	int cpos;
	LIST_OF(struct input_history_entry) history;
	struct input_history_entry *cur_hist;
};

void
add_dlg_field_do(struct dialog *dlg, enum widget_type type, unsigned char *label,
		 int min, int max, widget_handler_T *handler,
		 int data_len, void *data,
		 struct input_history *history, enum inpfield_flags flags);

#define add_dlg_field(dlg, label, min, max, handler, len, field, history)	\
	add_dlg_field_do(dlg, WIDGET_FIELD, label, min, max, handler, len, field, history, INPFIELD_NONE)

#define add_dlg_field_float(dlg, label, min, max, handler, len, field, history)	\
	add_dlg_field_do(dlg, WIDGET_FIELD, label, min, max, handler, len, field, history, INPFIELD_FLOAT)

#define add_dlg_field_float2(dlg, label, min, max, handler, len, field, history)	\
	add_dlg_field_do(dlg, WIDGET_FIELD, label, min, max, handler, len, field, history, INPFIELD_FLOAT2)

#define add_dlg_field_pass(dlg, label, min, max, handler, len, field)	\
	add_dlg_field_do(dlg, WIDGET_FIELD_PASS, label, min, max, handler, len, field, NULL, INPFIELD_NONE)

#define add_dlg_field_float_pass(dlg, label, min, max, handler, len, field)	\
	add_dlg_field_do(dlg, WIDGET_FIELD_PASS, label, min, max, handler, len, field, NULL, INPFIELD_FLOAT)


extern const struct widget_ops field_ops;
extern const struct widget_ops field_pass_ops;

widget_handler_status_T check_number(struct dialog_data *, struct widget_data *);
widget_handler_status_T check_nonempty(struct dialog_data *, struct widget_data *);

void dlg_format_field(struct terminal *, struct widget_data *, int, int *, int, int *, enum format_align, int format_only);

void input_field(struct terminal *, struct memory_list *, int, unsigned char *,
		 unsigned char *, unsigned char *, unsigned char *, void *,
		 struct input_history *, int, unsigned char *, int, int,
		 widget_handler_T *check,
		 void (*)(void *, unsigned char *),
		 void (*)(void *));

void
input_dialog(struct terminal *term, struct memory_list *ml,
	     unsigned char *title,
	     unsigned char *text,
	     void *data, struct input_history *history, int l,
	     unsigned char *def, int min, int max,
	     widget_handler_T *check,
	     void (*fn)(void *, unsigned char *),
	     void (*cancelfn)(void *));


/* Input lines */

#define INPUT_LINE_BUFFER_SIZE	256
#define INPUT_LINE_WIDGETS	1

enum input_line_code {
	INPUT_LINE_CANCEL,
	INPUT_LINE_PROCEED,
	INPUT_LINE_REWIND,
};

struct input_line;

/* If the handler returns non zero value it means to cancel the input line */
typedef enum input_line_code (*input_line_handler_T)(struct input_line *line, int action_id);

struct input_line {
	struct session *ses;
	input_line_handler_T handler;
	void *data;
	unsigned char buffer[INPUT_LINE_BUFFER_SIZE];
};

void
input_field_line(struct session *ses, unsigned char *prompt, void *data,
		 struct input_history *history, input_line_handler_T handler);

#define widget_has_history(widget_data) ((widget_data)->widget->type == WIDGET_FIELD \
					 && (widget_data)->widget->info.field.history)

#define widget_is_textfield(widget_data) ((widget_data)->widget->type == WIDGET_FIELD \
					  || (widget_data)->widget->type == WIDGET_FIELD_PASS)

#endif
