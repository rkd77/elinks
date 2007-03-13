#ifndef EL__DOCUMENT_FORMS_H
#define EL__DOCUMENT_FORMS_H

#include "util/lists.h"

struct document;
struct menu_item;



enum form_method {
	FORM_METHOD_GET,
	FORM_METHOD_POST,
	FORM_METHOD_POST_MP,
	FORM_METHOD_POST_TEXT_PLAIN,
};

struct form {
	LIST_HEAD(struct form);

	/* The value of @form_num serves both as a unique ID of the form.
	 * However @form_num and @form_end also stores information about where
	 * in the source the form is positioned. Combined they are used to
	 * figured which form items belong to which forms after rendering
	 * tables.
	 *
	 * Initially the range between @form_num and @form_end will stretch from
	 * 0 to INT_MAX. When a new form is added the range is partitioned so
	 * the forms each has unique source ranges. */
	int form_num;
	int form_end;

	unsigned char *action;
	unsigned char *name;
	unsigned char *onsubmit;
	unsigned char *target;
	enum form_method method;

	struct list_head items; /* -> struct form_control */
};



enum form_type {
	FC_TEXT,
	FC_PASSWORD,
	FC_FILE,
	FC_TEXTAREA,
	FC_CHECKBOX,
	FC_RADIO,
	FC_SELECT,
	FC_SUBMIT,
	FC_IMAGE,
	FC_RESET,
	FC_BUTTON,
	FC_HIDDEN,
};

enum form_mode {
	FORM_MODE_NORMAL,
	FORM_MODE_READONLY,
	FORM_MODE_DISABLED,
};

#define form_field_is_readonly(field) ((field)->mode != FORM_MODE_NORMAL)

enum form_wrap {
	FORM_WRAP_NONE,
	FORM_WRAP_SOFT,
	FORM_WRAP_HARD,
};

struct form_control {
	LIST_HEAD(struct form_control);

	struct form *form;
	int g_ctrl_num;

	/* The value of @position is relative to the place of the form item in
	 * the source. */
	int position;

	enum form_type type;
	enum form_mode mode;

	unsigned char *id; /* used by scripts */
	unsigned char *name;
	unsigned char *alt;
	unsigned char *default_value;
	/* For FC_SELECT. I have not found a better place for it. --witekfl */
	unsigned char *onchange;
	int default_state;
	int size;
	int cols, rows;
	enum form_wrap wrap;
	int maxlength;
	int nvalues;
	unsigned char **values;
	/* For FC_SELECT: If CONFIG_UTF8 is defined and UTF-8 I/O is
	 * enabled for the terminal, then @labels is in UTF-8;
	 * otherwise, @labels is in the charset of the terminal, and
	 * the charset is assumed to be unibyte.  (Thus, if you choose
	 * UTF-8 as the charset but disable UTF-8 I/O, you lose.)  The
	 * charset of the document has no effect here.  */
	unsigned char **labels;
	struct menu_item *menu;
};

/* Numerical form type <-> form type name */
int str2form_type(unsigned char *s);
unsigned char *form_type2str(enum form_type num);

struct form *init_form(void);
void done_form(struct form *form);
int has_form_submit(struct form *form);

int get_form_control_link(struct document *document, struct form_control *fc);
void done_form_control(struct form_control *fc);

#endif
