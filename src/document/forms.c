/** The document base functionality
 * @file */

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
#include "document/renderer.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"


struct form_type_name {
	enum form_type num;
	unsigned char *name;
};

static struct form_type_name form_type2name[] = {
	{ FC_TEXT,	"text"		},
	{ FC_PASSWORD,	"password"	},
	{ FC_FILE,	"file"		},
	{ FC_TEXTAREA,	"textarea"	},
	{ FC_CHECKBOX,	"checkbox"	},
	{ FC_RADIO,	"radio"		},
	{ FC_SELECT,	"select"	},
	{ FC_SUBMIT,	"submit"	},
	{ FC_IMAGE,	"image"		},
	{ FC_HIDDEN,	"hidden"	},
};

#define FORM_TYPE_COUNT (sizeof(form_type2name)/sizeof(struct form_type_name))

int
str2form_type(unsigned char *s)
{
	int n;

	for (n = 0; n < FORM_TYPE_COUNT; n++)
		if (!strcmp(form_type2name[n].name, s))
			return form_type2name[n].num;

	return -1;
}

unsigned char *
form_type2str(enum form_type num)
{
	int n;

	for (n = 0; n < FORM_TYPE_COUNT; n++)
		if (form_type2name[n].num == num)
			return form_type2name[n].name;

	return NULL;
}

#undef FORM_TYPE_COUNT


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
	mem_free_if(form->onsubmit);
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

	assert(form);
	if_assert_failed return 0;

	assertm(!list_empty(form->items), "form has no items");

	foreach (fc, form->items) {
		if (fc->type == FC_SUBMIT || fc->type == FC_IMAGE)
			return 1;
	}

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

	if (!document->links_sorted) sort_links(document);

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

	mem_free_if(fc->id);
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
