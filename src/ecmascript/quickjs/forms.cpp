/* The SpiderMonkey window object implementation. */

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

static std::map<void *, JSValueConst> map_forms;
static std::map<JSValueConst, void *> map_rev_forms;

#if 0
static void *
forms_GetOpaque(JSValueConst this_val)
{
	return map_rev_forms[this_val];
}
#endif

static void
forms_SetOpaque(JSValueConst this_val, void *node)
{
	if (!node) {
		map_rev_forms.erase(this_val);
	} else {
		map_rev_forms[this_val] = node;
	}
}

/* Find the form whose name is @name, which should normally be a
 * string (but might not be).  */
static JSValue
js_find_form_by_name(JSContext *ctx,
		  struct document_view *doc_view,
		  const char *string)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form *form;

	if (!*string)
		return JS_NULL;

	foreach (form, doc_view->document->forms) {
		if (form->name && !c_strcasecmp(string, form->name)) {
			return js_get_form_object(ctx, JS_NULL, form);
		}
	}

	return JS_NULL;
}

static void
js_forms_set_items(JSContext *ctx, JSValueConst this_val, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	struct view_state *vs;
	struct document_view *doc_view;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	int counter = 0;
	struct form_view *fv;

	foreach (fv, vs->forms) {
		struct form *form = find_form_by_form_view(document, fv);
		JSValue v = js_get_form_object(ctx, JS_NULL, form);
		JS_SetPropertyUint32(ctx, this_val, counter, v);

		if (form->name) {
			if (strcmp(form->name, "item") && strcmp(form->name, "namedItem")) {
				JS_SetPropertyStr(ctx, this_val, form->name, v);
			}
		}
		counter++;
	}
}

static JSValue
js_forms_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	return JS_NewInt32(ctx, list_size(&document->forms));
}

static JSValue
js_forms_item2(JSContext *ctx, JSValueConst this_val, int index)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct form_view *fv;
	int counter = -1;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

	vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			struct form *form = find_form_by_form_view(document, fv);

			return js_get_form_object(ctx, JS_NULL, form);
		}
	}

	return JS_UNDEFINED;
}

/* @forms_funcs{"item"} */
static JSValue
js_forms_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_UNDEFINED;
	}
	int index;

	if (JS_ToInt32(ctx, &index, argv[0])) {
		return JS_UNDEFINED;
	}

	return js_forms_item2(ctx, this_val, index);
}

/* @forms_funcs{"namedItem"} */
static JSValue
js_forms_namedItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_UNDEFINED;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	JSValue ret = js_find_form_by_name(ctx, doc_view, str);
	JS_FreeCString(ctx, str);

	RETURN_JS(ret);
}

#if 0
JSString *
unicode_to_jsstring(JSContext *ctx, unicode_val_T u)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	char16_t buf[2];

	/* This is supposed to make a string from which
	 * jsval_to_accesskey() can get the original @u back.
	 * If @u is a surrogate, then that is not possible, so
	 * return NULL to indicate an error instead.
	 *
	 * If JS_NewUCStringCopyN hits a null character, it truncates
	 * the string there and pads it with more nulls.  However,
	 * that is not a problem here, because if there is a null
	 * character in buf[], then it must be the only character.  */
	if (u <= 0xFFFF && !is_utf16_surrogate(u)) {
		buf[0] = u;
		return JS_NewUCStringCopyN(ctx, buf, 1);
	} else if (needs_utf16_surrogates(u)) {
		buf[0] = get_utf16_high_surrogate(u);
		buf[1] = get_utf16_low_surrogate(u);
		return JS_NewUCStringCopyN(ctx, buf, 2);
	} else {
		return NULL;
	}
}
#endif

static JSValue
js_forms_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return JS_NewString(ctx, "[forms object]");
}

static const JSCFunctionListEntry js_forms_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_forms_get_property_length, nullptr),
	JS_CFUNC_DEF("item", 1, js_forms_item),
	JS_CFUNC_DEF("namedItem", 1, js_forms_namedItem),
	JS_CFUNC_DEF("toString", 0, js_forms_toString)
};

#if 0
static JSValue
js_forms_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_forms_class_id);
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
js_forms_init(JSContext *ctx, JSValue global_obj)
{
	JSValue forms_proto, forms_class;

	/* create the forms class */
	JS_NewClassID(&js_forms_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_forms_class_id, &js_forms_class);

	forms_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, forms_proto, js_forms_proto_funcs, countof(js_forms_proto_funcs));

	forms_class = JS_NewCFunction2(ctx, js_forms_ctor, "forms", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, forms_class, forms_proto);
	JS_SetClassProto(ctx, js_forms_class_id, forms_proto);

	JS_SetPropertyStr(ctx, global_obj, "forms", forms_proto);
	return 0;
}
#endif

JSValue
getForms(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	auto node_find = map_forms.find(node);

	if (node_find != map_forms.end()) {
		JSValue r = JS_DupValue(ctx, node_find->second);
		RETURN_JS(r);
	}
	JSValue forms_obj = JS_NewArray(ctx);
	JS_SetPropertyFunctionList(ctx, forms_obj, js_forms_proto_funcs, countof(js_forms_proto_funcs));
	forms_SetOpaque(forms_obj, node);
	js_forms_set_items(ctx, forms_obj, node);
	map_forms[node] = forms_obj;

	JSValue rr = JS_DupValue(ctx, forms_obj);
	RETURN_JS(rr);
}
