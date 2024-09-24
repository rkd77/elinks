/* The MuJS forms implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/mujs/mapa.h"
#include "js/mujs.h"
#include "js/mujs/document.h"
#include "js/mujs/form.h"
#include "js/mujs/forms.h"
#include "js/mujs/input.h"
#include "js/mujs/window.h"
#include "viewer/text/form.h"
#include "viewer/text/vs.h"

void *map_forms;
void *map_rev_forms;

/* Find the form whose name is @name, which should normally be a
 * string (but might not be).  */
static void
mjs_find_form_by_name(js_State *J,
		  struct document_view *doc_view,
		  const char *string)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form *form;

	if (!*string) {
		js_pushnull(J);
		return;
	}

	foreach (form, doc_view->document->forms) {
		if (form->name && !c_strcasecmp(string, form->name)) {
			mjs_push_form_object(J, form);
			return;
		}
	}
	js_pushnull(J);
}

static void
mjs_forms_set_items(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	struct view_state *vs;
	struct document_view *doc_view;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	int counter = 0;
	struct form_view *fv;

	foreach (fv, vs->forms) {
		struct form *form = find_form_by_form_view(document, fv);

		if (!form) {
			continue;
		}
		mjs_push_form_object(J, form);
		js_setindex(J, -2, counter);

		if (form->name) {
			if (js_try(J)) {
				js_pop(J, 1);
				goto next;
			}
			mjs_push_form_object(J, form);
			js_setproperty(J, -2, form->name);
			js_endtry(J);
		}
next:
		counter++;
	}
}

static void
mjs_forms_get_property_length(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	js_pushnumber(J, list_size(&document->forms));
}

static void
mjs_forms_item2(js_State *J, int index)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form_view *fv;
	int counter = -1;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			struct form *form = find_form_by_form_view(document, fv);

			mjs_push_form_object(J, form);
			return;
		}
	}
	js_pushundefined(J);
}

/* @forms_funcs{"item"} */
static void
mjs_forms_item(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int index = js_toint32(J, 1);;

	mjs_forms_item2(J, index);
}

/* @forms_funcs{"namedItem"} */
static void
mjs_forms_namedItem(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	const char *str = js_tostring(J, 1);;

	if (!str) {
		js_error(J, "!str");
		return;
	}
	mjs_find_form_by_name(J, doc_view, str);
}

static void
mjs_forms_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[forms object]");
}

void
mjs_push_forms(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	js_newarray(J);
	{
		js_newuserdata(J, "forms", node, NULL);

		addmethod(J, "item", mjs_forms_item, 1);
		addmethod(J, "namedItem", mjs_forms_namedItem, 1);
		addmethod(J, "toString", mjs_forms_toString, 0);

		addproperty(J, "length", mjs_forms_get_property_length, NULL);
		mjs_forms_set_items(J);
	}
	attr_save_in_map(map_forms, node, node);
}
