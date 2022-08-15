/* The MuJS form object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/document.h"
#include "ecmascript/mujs/form.h"
#include "ecmascript/mujs/forms.h"
#include "ecmascript/mujs/input.h"
#include "ecmascript/mujs/window.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#include <libxml++/libxml++.h>
#include <map>

static std::map<struct form_view *, void *> map_form_elements;
static std::map<void *, struct form_view *> map_elements_form;
static std::map<struct form *, void *> map_form;
static std::map<void *, struct form *> map_rev_form;

//void mjs_push_form_object(js_State *J, struct form *form);

static void
mjs_push_form_control_object(js_State *J,
			enum form_type type, struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	switch (type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
		case FC_CHECKBOX:
		case FC_RADIO:
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_BUTTON:
		case FC_HIDDEN:
		case FC_SELECT:
			mjs_push_input_object(J, fs);
			return;

		case FC_TEXTAREA:
			/* TODO */
			js_pushnull(J);
			return;

		default:
			INTERNAL("Weird fc->type %d", type);
			js_pushnull(J);
			return;
	}
}

static void
mjs_form_set_items(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = (struct form_view *)node;
	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return; /* detached */
	}
	form = find_form_by_form_view(document, form_view);

	int counter = 0;
	struct el_form_control *fc;
	foreach (fc, form->items) {
		struct form_state *fs = find_form_state(doc_view, fc);

		if (!fs) {
			continue;
		}
		mjs_push_form_control_object(J, fc->type, fs);
		js_dup(J);
		js_setindex(J, 0, counter);

		if (fc->id && strcmp(fc->id, "item") && strcmp(fc->id, "namedItem")) {
			js_setproperty(J, 0, fc->id);
		} else if (fc->name && strcmp(fc->name, "item") && strcmp(fc->name, "namedItem")) {
			js_setproperty(J, 0, fc->name);
		} else {
			js_pop(J, 1);
		}
		counter++;
	}
}

static void
mjs_form_set_items2(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	form = (struct form *)node;

	int counter = 0;
	struct el_form_control *fc;
	foreach (fc, form->items) {
		struct form_state *fs = find_form_state(doc_view, fc);

		if (!fs) {
			continue;
		}
		mjs_push_form_control_object(J, fc->type, fs);
		js_dup(J);
		js_setindex(J, 0, counter);

		if (fc->id && strcmp(fc->id, "item") && strcmp(fc->id, "namedItem")) {
			js_setproperty(J, 0, fc->id);
		} else if (fc->name && strcmp(fc->name, "item") && strcmp(fc->name, "namedItem")) {
			js_setproperty(J, 0, fc->name);
		} else {
			js_pop(J, 1);
		}
		counter++;
	}
}

static void
mjs_form_elements_get_property_length(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}

	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = (struct form_view *)js_touserdata(J, 0, "form_view");
	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return; /* detached */
	}
	form = find_form_by_form_view(document, form_view);
	js_pushnumber(J, list_size(&form->items));
}

static void
mjs_form_elements_item2(js_State *J, int index)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct el_form_control *fc;
	int counter = -1;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = (struct form_view *)js_touserdata(J, 0, "form_view");

	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	form = find_form_by_form_view(document, form_view);
	foreach (fc, form->items) {
		counter++;

		if (counter == index) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				mjs_push_form_control_object(J, fc->type, fs);
				return;
			}
		}
	}
	js_pushundefined(J);
}

/* @form_elements_funcs{"item"} */
static void
mjs_form_elements_item(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int index = js_toint32(J, 1);

	mjs_form_elements_item2(J, index);
}

static void
mjs_form_elements_namedItem2(js_State *J, const char *string)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	if (!*string) {
		js_pushundefined(J);
		return;
	}
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = (struct form_view *)js_touserdata(J, 0, "form_view");

	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	form = find_form_by_form_view(document, form_view);

	foreach (fc, form->items) {
		if ((fc->id && !c_strcasecmp(string, fc->id))
		    || (fc->name && !c_strcasecmp(string, fc->name))) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				mjs_push_form_control_object(J, fc->type, fs);
				return;
			}
		}
	}
	js_pushundefined(J);
}

/* @form_elements_funcs{"namedItem"} */
static void
mjs_form_elements_namedItem(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	mjs_form_elements_namedItem2(J, str);
}

static void
mjs_form_elements_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[form elements object]");
}

static void
mjs_form_get_property_action(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);

	js_pushstring(J, form->action);
}

static void
mjs_form_set_property_action(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);

	const char *str = js_tostring(J, 1);
	char *string;

	if (!str) {
		js_pushnull(J);
		return;
	}
	string = stracpy(str);

	if (form->action) {
		ecmascript_set_action(&form->action, string);
	} else {
		mem_free_set(&form->action, string);
	}
	js_pushundefined(J);
}

#if 0
void
mjs_detach_form_view(struct form_view *fv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue jsform = fv->ecmascript_obj;

	if (!JS_IsNull(jsform)) {
		map_form_elements.erase(fv);
		setOpaque(jsform, nullptr);
		fv->ecmascript_obj = JS_NULL;
	}
}
#endif

static void
mjs_elements_finalizer(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_view *fv = (struct form_view *)node;

	map_form_elements.erase(fv);
}

void
mjs_push_form_elements(js_State *J, struct form_view *fv)
{
	js_newobject(J);
	{
		js_newuserdata(J, "form_view", fv, mjs_elements_finalizer);

		addmethod(J, "item", mjs_form_elements_item, 1);
		addmethod(J, "namedItem", mjs_form_elements_namedItem, 1);
		addmethod(J, "toString", mjs_form_elements_toString, 0);

		addproperty(J, "length", mjs_form_elements_get_property_length, NULL);

		mjs_form_set_items(J, fv);
	}
	map_form_elements[fv] = fv;
}

static void
mjs_form_get_property_elements(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	struct form *form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);

	struct form_view *fv = NULL;
	bool found = false;

	foreach (fv, vs->forms) {
		if (form->form_num == fv->form_num) {
			found = true;
			break;
		}
	}

	if (!found || !fv) {
#ifdef ECMASCRIPT_DEBUG
		fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	mjs_push_form_elements(J, fv);
}

static void
mjs_form_get_property_encoding(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);

	switch (form->method) {
	case FORM_METHOD_GET:
	case FORM_METHOD_POST:
		js_pushstring(J, "application/x-www-form-urlencoded");
		return;
	case FORM_METHOD_POST_MP:
		js_pushstring(J, "multipart/form-data");
		return;
	case FORM_METHOD_POST_TEXT_PLAIN:
		js_pushstring(J, "text/plain");
		return;
	}
	js_pushundefined(J);
}

/* @form_class.setProperty */
static void
mjs_form_set_property_encoding(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}

	if (!c_strcasecmp(str, "application/x-www-form-urlencoded")) {
		form->method = form->method == FORM_METHOD_GET ? FORM_METHOD_GET
		                                               : FORM_METHOD_POST;
	} else if (!c_strcasecmp(str, "multipart/form-data")) {
		form->method = FORM_METHOD_POST_MP;
	} else if (!c_strcasecmp(str, "text/plain")) {
		form->method = FORM_METHOD_POST_TEXT_PLAIN;
	}
	js_pushundefined(J);
}

static void
mjs_form_get_property_length(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);
	js_pushnumber(J, list_size(&form->items));
}

static void
mjs_form_get_property_method(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);

	switch (form->method) {
	case FORM_METHOD_GET:
		js_pushstring(J, "GET");
		return;

	case FORM_METHOD_POST:
	case FORM_METHOD_POST_MP:
	case FORM_METHOD_POST_TEXT_PLAIN:
		js_pushstring(J, "POST");
		return;
	}
	js_pushundefined(J);
}

/* @form_class.setProperty */
static void
mjs_form_set_property_method(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}

	if (!c_strcasecmp(str, "GET")) {
		form->method = FORM_METHOD_GET;
	} else if (!c_strcasecmp(str, "POST")) {
		form->method = FORM_METHOD_POST;
	}
	js_pushundefined(J);
}

static void
mjs_form_get_property_name(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);
	js_pushstring(J, form->name);
}

/* @form_class.setProperty */
static void
mjs_form_set_property_name(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);

	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	mem_free_set(&form->name, stracpy(str));
	js_pushundefined(J);
}

static void
mjs_form_get_property_target(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);
	js_pushstring(J, form->target);
}

static void
mjs_form_set_property_target(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);

	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	mem_free_set(&form->target, stracpy(str));
	js_pushundefined(J);
}

/* @form_funcs{"reset"} */
static void
mjs_form_reset(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);

	do_reset_form(doc_view, form);
	draw_forms(doc_view->session->tab->term, doc_view);

	js_pushundefined(J);
}

/* @form_funcs{"submit"} */
static void
mjs_form_submit(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	struct form *form;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	ses = doc_view->session;

	form = (struct form *)js_touserdata(J, 0, "form");
	assert(form);
	submit_given_form(ses, doc_view, form, 0);

	js_pushundefined(J);
}

static void
mjs_form_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[form object]");
}

static
void mjs_form_finalizer(js_State *J, void *node)
{
	struct form *form = (struct form *)node;
	map_form.erase(form);
}

void
mjs_push_form_object(js_State *J, struct form *form)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newobject(J);
	{
		js_newuserdata(J, "form", form, mjs_form_finalizer);

		addmethod(J, "reset", mjs_form_reset, 0);
		addmethod(J, "submit", mjs_form_submit, 0);
		addmethod(J, "toString", mjs_form_toString, 0);

		addproperty(J, "action", mjs_form_get_property_action, mjs_form_set_property_action);
		addproperty(J, "elements", mjs_form_get_property_elements, NULL);
		addproperty(J, "encoding", mjs_form_get_property_encoding, mjs_form_set_property_encoding);
		addproperty(J, "length", mjs_form_get_property_length, NULL);
		addproperty(J, "method", mjs_form_get_property_method, mjs_form_set_property_method);
		addproperty(J, "name", mjs_form_get_property_name, mjs_form_set_property_name);
		addproperty(J, "target", mjs_form_get_property_target, mjs_form_set_property_target);

		mjs_form_set_items2(J, form);
	}
	map_form[form] = form;
}
