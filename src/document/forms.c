/* The document base functionality */
/* $Id: forms.c,v 1.6.2.3 2005/04/05 21:08:40 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listmenu.h"
#include "document/document.h"
#include "document/forms.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"


struct form *
init_form(void)
{
	struct form *form = mem_calloc(1, sizeof(*form));

	if (!form) return NULL;

	/* Make the form initially stretch the whole range. */
	form->form_end = INT_MAX;

	init_list(form->items);

	return form;
}

void
done_form(struct form *form)
{
	struct form_control *fc;

	if (form->next)
		del_from_list(form);

	mem_free_if(form->action);
	mem_free_if(form->name);
	mem_free_if(form->target);

	foreach (fc, form->items) {
		done_form_control(fc);
	}
	free_list(form->items);

	mem_free(form);
}

int
has_form_submit(struct form *form)
{
	struct form_control *fc;
	int found = 0;

	assert(form);
	if_assert_failed return 0;

	foreach (fc, form->items) {
		if (fc->type == FC_SUBMIT || fc->type == FC_IMAGE)
			return 1;
		found = 1;
	}

	assertm(found, "form is not on list");
	/* Return path :-). */
	return 0;
}


int
get_form_control_link(struct document *document, struct form_control *fc)
{
	int link;

	/* Hidden form fields have no links. */
	if (fc->type == FC_HIDDEN)
		return -1;

	for (link = 0; link < document->nlinks; link++)
		if (fc == get_link_form_control(&document->links[link]))
			return link;

	assertm(0, "Form control has no link.");

	return -1;
}

void
done_form_control(struct form_control *fc)
{
	int i;

	assert(fc);
	if_assert_failed return;

	mem_free_if(fc->name);
	mem_free_if(fc->alt);
	mem_free_if(fc->default_value);

	for (i = 0; i < fc->nvalues; i++) {
		mem_free_if(fc->values[i]);
		mem_free_if(fc->labels[i]);
	}

	mem_free_if(fc->values);
	mem_free_if(fc->labels);
	if (fc->menu) free_menu(fc->menu);
}
