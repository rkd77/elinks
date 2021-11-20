/* The QuickJS form object implementation. */

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
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/document.h"
#include "ecmascript/quickjs/form.h"
#include "ecmascript/quickjs/forms.h"
#include "ecmascript/quickjs/input.h"
#include "ecmascript/quickjs/window.h"
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

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static std::map<struct form_view *, JSValueConst> map_form_elements;
static std::map<JSValueConst, struct form_view *> map_elements_form;
static std::map<struct form *, JSValueConst> map_form;
static std::map<JSValueConst, struct form *> map_rev_form;

JSValue getForm(JSContext *ctx, struct form *form);

static struct form_view *
getOpaque(JSValueConst this_val)
{
	return map_elements_form[this_val];
}

static void
setOpaque(JSValueConst this_val, struct form_view *fv)
{
	if (!fv) {
		map_elements_form.erase(this_val);
	} else {
		map_elements_form[this_val] = fv;
	}
}

static struct form *
form_GetOpaque(JSValueConst this_val)
{
	return map_rev_form[this_val];
}

static void
form_SetOpaque(JSValueConst this_val, struct form *form)
{
	if (!form) {
		map_rev_form.erase(this_val);
	} else {
		map_rev_form[this_val] = form;
	}
}

static JSValue
js_get_form_control_object(JSContext *ctx,
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
			return js_get_input_object(ctx, fs);

		case FC_TEXTAREA:
			/* TODO */
			return JS_NULL;

		default:
			INTERNAL("Weird fc->type %d", type);
			return JS_NULL;
	}
}

static void
js_form_set_items(JSContext *ctx, JSValueConst this_val, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;

	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = node;
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

		JSValue obj = js_get_form_control_object(ctx, fc->type, fs);
		JS_SetPropertyUint32(ctx, this_val, counter, JS_DupValue(ctx, obj));

		if (fc->id) {
			if (strcmp(fc->id, "item") && strcmp(fc->id, "namedItem")) {
				JS_SetPropertyStr(ctx, this_val, fc->id, JS_DupValue(ctx, obj));
			}
		} else if (fc->name) {
			if (strcmp(fc->name, "item") && strcmp(fc->name, "namedItem")) {
				JS_SetPropertyStr(ctx, this_val, fc->name, JS_DupValue(ctx, obj));
			}
		}
		counter++;
	}
}

static void
js_form_set_items2(JSContext *ctx, JSValueConst this_val, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form = node;

	int counter = 0;
	struct el_form_control *fc;
	foreach (fc, form->items) {
		struct form_state *fs = find_form_state(doc_view, fc);

		if (!fs) {
			continue;
		}

		JSValue obj = js_get_form_control_object(ctx, fc->type, fs);
		JS_SetPropertyUint32(ctx, this_val, counter, obj);

		if (fc->id) {
			if (strcmp(fc->id, "item") && strcmp(fc->id, "namedItem")) {
				JS_SetPropertyStr(ctx, this_val, fc->id, obj);
			}
		} else if (fc->name) {
			if (strcmp(fc->name, "item") && strcmp(fc->name, "namedItem")) {
				JS_SetPropertyStr(ctx, this_val, fc->name, obj);
			}
		}
		counter++;
	}
}

static JSValue
js_form_elements_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}

	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = getOpaque(this_val);
	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED; /* detached */
	}
	form = find_form_by_form_view(document, form_view);

	return JS_NewInt32(ctx, list_size(&form->items));
}

static JSValue
js_form_elements_item2(JSContext *ctx, JSValueConst this_val, int index)
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
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = getOpaque(this_val);

	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	form = find_form_by_form_view(document, form_view);
	foreach (fc, form->items) {
		counter++;

		if (counter == index) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				return js_get_form_control_object(ctx, fc->type, fs);
			}
		}
	}

	return JS_UNDEFINED;
}

/* @form_elements_funcs{"item"} */
static JSValue
js_form_elements_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_NULL;
	}

	int index;

	if (JS_ToInt32(ctx, &index, argv[0])) {
		return JS_NULL;
	}

	return js_form_elements_item2(ctx, this_val, index);
}

static JSValue
js_form_elements_namedItem2(JSContext *ctx, JSValueConst this_val, const char *string)
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
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);

	if (!*string) {
		return JS_UNDEFINED;
	}
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = getOpaque(this_val);

	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	form = find_form_by_form_view(document, form_view);

	foreach (fc, form->items) {
		if ((fc->id && !c_strcasecmp(string, fc->id))
		    || (fc->name && !c_strcasecmp(string, fc->name))) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				return js_get_form_control_object(ctx, fc->type, fs);
			}
		}
	}

	return JS_UNDEFINED;
}

/* @form_elements_funcs{"namedItem"} */
static JSValue
js_form_elements_namedItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_NULL;
	}

	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	JSValue ret = js_form_elements_namedItem2(ctx, this_val, str);
	JS_FreeCString(ctx, str);

	RETURN_JS(ret);
}
static struct form_view *
js_form_get_form_view(JSContext *ctx, JSValueConst this_val, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_view *fv = getOpaque(this_val);

	return fv;
}

static JSValue
js_form_get_property_action(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);

	JSValue r = JS_NewString(ctx, form->action);
	RETURN_JS(r);
}

static JSValue
js_form_set_property_action(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);

	const char *str;
	char *string;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}

	string = stracpy(str);
	JS_FreeCString(ctx, str);

	if (form->action) {
		ecmascript_set_action(&form->action, string);
	} else {
		mem_free_set(&form->action, string);
	}

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_form_elements_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_form_elements_get_property_length, nullptr),
	JS_CFUNC_DEF("item", 1, js_form_elements_item),
	JS_CFUNC_DEF("namedItem", 1, js_form_elements_namedItem),
};

void
quickjs_detach_form_view(struct form_view *fv)
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

static
void js_elements_finalizer(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_view *fv = getOpaque(val);

	setOpaque(val, nullptr);
	fv->ecmascript_obj = JS_NULL;
	map_form_elements.erase(fv);
}

static JSClassDef js_form_elements_class = {
	"elements",
	js_elements_finalizer
};

JSValue
getFormElements(JSContext *ctx, struct form_view *fv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	auto node_find = map_form_elements.find(fv);

	if (node_find != map_form_elements.end()) {
		JSValue r = JS_DupValue(ctx, node_find->second);
		RETURN_JS(r);
	}
	JSValue form_elements_obj = JS_NewArray(ctx);

	JS_SetPropertyFunctionList(ctx, form_elements_obj, js_form_elements_proto_funcs, countof(js_form_elements_proto_funcs));
	setOpaque(form_elements_obj, fv);
	fv->ecmascript_obj = form_elements_obj;
	js_form_set_items(ctx, form_elements_obj, fv);
	map_form_elements[fv] = form_elements_obj;

	JSValue rr = JS_DupValue(ctx, form_elements_obj);
	RETURN_JS(rr);
}

static JSValue
js_form_get_property_elements(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct form_view *fv = vs->forms.next;
	if (!fv) {
#ifdef ECMASCRIPT_DEBUG
		fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}

	return getFormElements(ctx, fv);
}

static JSValue
js_form_get_property_encoding(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);

	JSValue r;

	switch (form->method) {
	case FORM_METHOD_GET:
	case FORM_METHOD_POST:
		r = JS_NewString(ctx, "application/x-www-form-urlencoded");
		RETURN_JS(r);
	case FORM_METHOD_POST_MP:
		r = JS_NewString(ctx, "multipart/form-data");
		RETURN_JS(r);
	case FORM_METHOD_POST_TEXT_PLAIN:
		r = JS_NewString(ctx, "text/plain");
		RETURN_JS(r);
	}

	return JS_UNDEFINED;
}

/* @form_class.setProperty */
static JSValue
js_form_set_property_encoding(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}

	if (!c_strcasecmp(str, "application/x-www-form-urlencoded")) {
		form->method = form->method == FORM_METHOD_GET ? FORM_METHOD_GET
		                                               : FORM_METHOD_POST;
	} else if (!c_strcasecmp(str, "multipart/form-data")) {
		form->method = FORM_METHOD_POST_MP;
	} else if (!c_strcasecmp(str, "text/plain")) {
		form->method = FORM_METHOD_POST_TEXT_PLAIN;
	}
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_form_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);

	return JS_NewInt32(ctx, list_size(&form->items));
}

static JSValue
js_form_get_property_method(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);

	JSValue r;

	switch (form->method) {
	case FORM_METHOD_GET:
		r = JS_NewStringLen(ctx, "GET", 3);
		RETURN_JS(r);

	case FORM_METHOD_POST:
	case FORM_METHOD_POST_MP:
	case FORM_METHOD_POST_TEXT_PLAIN:
		r = JS_NewStringLen(ctx, "POST", 4);
		RETURN_JS(r);
	}

	return JS_UNDEFINED;
}

/* @form_class.setProperty */
static JSValue
js_form_set_property_method(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);
	const char *str;
	char *string;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}

	if (!c_strcasecmp(str, "GET")) {
		form->method = FORM_METHOD_GET;
	} else if (!c_strcasecmp(str, "POST")) {
		form->method = FORM_METHOD_POST;
	}
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_form_get_property_name(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);

	JSValue r = JS_NewString(ctx, form->name);
	RETURN_JS(r);
}

/* @form_class.setProperty */
static JSValue
js_form_set_property_name(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;

	form = form_GetOpaque(this_val);
	assert(form);

	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	mem_free_set(&form->name, stracpy(str));
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_form_get_property_target(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);

	JSValue r = JS_NewString(ctx, form->target);
	RETURN_JS(r);
}

static JSValue
js_form_set_property_target(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);

	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	mem_free_set(&form->target, stracpy(str));
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

/* @form_funcs{"reset"} */
static JSValue
js_form_reset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	form = form_GetOpaque(this_val);
	assert(form);

	do_reset_form(doc_view, form);
	draw_forms(doc_view->session->tab->term, doc_view);

	return JS_FALSE;
}

/* @form_funcs{"submit"} */
static JSValue
js_form_submit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	struct form_view *fv;
	struct form *form;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	ses = doc_view->session;

	form = form_GetOpaque(this_val);
	assert(form);
	submit_given_form(ses, doc_view, form, 0);

	return JS_FALSE;
}


JSValue
js_get_form_object(JSContext *ctx, JSValueConst jsdoc, struct form *form)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return getForm(ctx, form);
}

#if 0
static JSValue
js_elements_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_form_elements_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	RETURN_JS(obj);

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_elements_init(JSContext *ctx, JSValue global_obj)
{
	JSValue elements_proto, elements_class;

	/* create the elements class */
	JS_NewClassID(&js_form_elements_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_form_elements_class_id, &js_form_elements_class);

	elements_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, elements_proto, js_form_elements_proto_funcs, countof(js_form_elements_proto_funcs));

	elements_class = JS_NewCFunction2(ctx, js_elements_ctor, "elements", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, elements_class, elements_proto);
	JS_SetClassProto(ctx, js_form_elements_class_id, elements_proto);

	JS_SetPropertyStr(ctx, global_obj, "elements", elements_proto);
	return 0;
}
#endif

static const JSCFunctionListEntry js_form_proto_funcs[] = {
	JS_CGETSET_DEF("action", js_form_get_property_action, js_form_set_property_action),
	JS_CGETSET_DEF("elements", js_form_get_property_elements, nullptr),
	JS_CGETSET_DEF("encoding", js_form_get_property_encoding, js_form_set_property_encoding),
	JS_CGETSET_DEF("length", js_form_get_property_length, nullptr),
	JS_CGETSET_DEF("method", js_form_get_property_method, js_form_set_property_method),
	JS_CGETSET_DEF("name", js_form_get_property_name, js_form_set_property_name),
	JS_CGETSET_DEF("target", js_form_get_property_target, js_form_set_property_target),
	JS_CFUNC_DEF("reset", 0, js_form_reset),
	JS_CFUNC_DEF("submit", 0, js_form_submit),
};


static
void js_form_finalizer(JSRuntime *rt, JSValue val)
{
	struct form *form = form_GetOpaque(val);

	form_SetOpaque(val, nullptr);
	form->ecmascript_obj = JS_NULL;
	map_form.erase(form);
}

static JSClassDef js_form_class = {
	"form",
	js_form_finalizer
};

#if 0
static JSValue
js_form_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_form_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	RETURN_JS(obj);

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_form_init(JSContext *ctx, JSValue global_obj)
{
	JSValue form_proto, form_class;

	/* create the form class */
	JS_NewClassID(&js_form_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_form_class_id, &js_form_class);

	form_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, form_proto, js_form_proto_funcs, countof(js_form_proto_funcs));

	form_class = JS_NewCFunction2(ctx, js_form_ctor, "form", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, form_class, form_proto);
	JS_SetClassProto(ctx, js_form_class_id, form_proto);

	JS_SetPropertyStr(ctx, global_obj, "form", form_proto);
	return 0;
}
#endif

JSValue
getForm(JSContext *ctx, struct form *form)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	auto node_find = map_form.find(form);

	if (node_find != map_form.end()) {
		JSValue r = JS_DupValue(ctx, node_find->second);
		RETURN_JS(r);
	}
	JSValue form_obj = JS_NewArray(ctx);

	JS_SetPropertyFunctionList(ctx, form_obj, js_form_proto_funcs, countof(js_form_proto_funcs));
	form_SetOpaque(form_obj, form);
	js_form_set_items2(ctx, form_obj, form);
	form->ecmascript_obj = form_obj;

	map_form[form] = form_obj;

	JSValue rr = JS_DupValue(ctx, form_obj);
	RETURN_JS(rr);
}
