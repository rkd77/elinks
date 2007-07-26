#ifndef EL__FORMHIST_FORMHIST_H
#define EL__FORMHIST_FORMHIST_H

#include "document/forms.h"
#include "main/module.h"
#include "main/object.h"
#include "session/session.h"
#include "util/lists.h"

struct formhist_data {
	OBJECT_HEAD(struct formhist_data);

	/* List of submitted_values for this form */
	LIST_OF(struct submitted_value) *submit;

	struct listbox_item *box_item;

	/* Whether to save this form or not. */
	unsigned int dontsave:1;

	/* <action> URI for this form. Must be at end of struct. */
	unsigned char url[1];
};

/* Look up @name form of @url document in the form history. Returns the saved
 * value if present, NULL upon an error. */
unsigned char *get_form_history_value(unsigned char *url, unsigned char *name);

void memorize_form(struct session *ses,
		   LIST_OF(struct submitted_value) *submit,
		   struct form *forminfo);

int save_formhist_to_file(void);
void delete_formhist_item(struct formhist_data *form);
int load_formhist_from_file(void);

extern struct module forms_history_module;

#endif /* EL__FORMHIST_FORMHIST_H */
