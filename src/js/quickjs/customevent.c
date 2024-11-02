/* The QuickJS CustomEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/libdom/dom.h"
#include "js/quickjs.h"
#include "js/quickjs/element.h"
#include "js/quickjs/event.h"
#include "js/quickjs/node.h"
#include "intl/charsets.h"
#include "terminal/event.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_customEvent_class_id;

static JSValue js_customEvent_get_property_bubbles(JSContext *ctx, JSValueConst this_val);
static JSValue js_customEvent_get_property_cancelable(JSContext *ctx, JSValueConst this_val);
//static JSValue js_customEvent_get_property_composed(JSContext *ctx, JSValueConst this_val);
static JSValue js_customEvent_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val);
static JSValue js_customEvent_get_property_detail(JSContext *ctx, JSValueConst this_val);
static JSValue js_customEvent_get_property_target(JSContext *ctx, JSValueConst this_val);
static JSValue js_customEvent_get_property_type(JSContext *ctx, JSValueConst this_val);

static JSValue js_customEvent_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

static
void js_customEvent_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	dom_custom_event *event = (dom_custom_event *)JS_GetOpaque(val, js_customEvent_class_id);

	if (event) {
		JSValue *detail = NULL;
		(void)dom_custom_event_get_detail(event, &detail);

		if (detail) {
			free(detail);
		}

		dom_event_unref(event);
	}
}

static JSClassDef js_customEvent_class = {
	"CustomEvent",
	js_customEvent_finalizer
};

static const JSCFunctionListEntry js_customEvent_proto_funcs[] = {
	JS_CGETSET_DEF("bubbles",	js_customEvent_get_property_bubbles, NULL),
	JS_CGETSET_DEF("cancelable",	js_customEvent_get_property_cancelable, NULL),
//	JS_CGETSET_DEF("composed",	js_customEvent_get_property_composed, NULL),
	JS_CGETSET_DEF("defaultPrevented",	js_customEvent_get_property_defaultPrevented, NULL),
	JS_CGETSET_DEF("detail",	js_customEvent_get_property_detail, NULL),
	JS_CGETSET_DEF("target",	js_customEvent_get_property_target, NULL),
	JS_CGETSET_DEF("type",	js_customEvent_get_property_type, NULL),
	JS_CFUNC_DEF("preventDefault", 0, js_customEvent_preventDefault),
};

static JSValue
js_customEvent_get_property_bubbles(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_custom_event *event = (dom_custom_event *)(JS_GetOpaque(this_val, js_customEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_event_ref(event);
	bool bubbles = false;
	(void)dom_event_get_bubbles(event, &bubbles);
	JSValue r = JS_NewBool(ctx, bubbles);
	dom_event_unref(event);

	RETURN_JS(r);
}

static JSValue
js_customEvent_get_property_cancelable(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_custom_event *event = (dom_custom_event *)(JS_GetOpaque(this_val, js_customEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_event_ref(event);

	bool cancelable = false;
	(void)dom_event_get_cancelable(event, &cancelable);
	JSValue r = JS_NewBool(ctx, cancelable);
	dom_event_unref(event);

	RETURN_JS(r);
}

#if 0
static JSValue
js_customEvent_get_property_composed(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljscustom_event *event = (struct eljscustom_event *)(JS_GetOpaque(this_val, js_customEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, event->composed);

	RETURN_JS(r);
}
#endif

static JSValue
js_customEvent_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_custom_event *event = (dom_custom_event *)(JS_GetOpaque(this_val, js_customEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_event_ref(event);
	bool prevented = false;
	(void)dom_event_is_default_prevented(event, &prevented);
	JSValue r = JS_NewBool(ctx, prevented);
	dom_event_unref(event);

	RETURN_JS(r);
}

static JSValue
js_customEvent_get_property_detail(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_custom_event *event = (dom_custom_event *)(JS_GetOpaque(this_val, js_customEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_event_ref(event);

	JSValue *detail = NULL;
	dom_exception exc = dom_custom_event_get_detail(event, &detail);

	if (exc != DOM_NO_ERR || !detail) {
		dom_event_unref(event);
		return JS_NULL;
	}
	dom_event_unref(event);

	RETURN_JS(*detail);
}

static JSValue
js_customEvent_get_property_target(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct dom_custom_event *event = (dom_custom_event *)(JS_GetOpaque(this_val, js_customEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_event_ref(event);
	dom_event_target *target = NULL;
	dom_exception exc = dom_event_get_target(event, &target);

	if (exc != DOM_NO_ERR || !target) {
		dom_event_unref(event);
		return JS_NULL;
	}
	JSValue r = getNode(ctx, target);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(target);
	dom_event_unref(event);

	RETURN_JS(r);
}

static JSValue
js_customEvent_get_property_type(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct dom_custom_event *event = (dom_custom_event *)(JS_GetOpaque(this_val, js_customEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_event_ref(event);

	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		JSValue r = JS_NewString(ctx, "");
		dom_event_unref(event);
		RETURN_JS(r);
	}
	JSValue r = JS_NewString(ctx, dom_string_data(typ));
	dom_string_unref(typ);
	dom_event_unref(event);

	RETURN_JS(r);
}

static JSValue
js_customEvent_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_custom_event *event = (dom_custom_event *)(JS_GetOpaque(this_val, js_customEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_event_ref(event);
	dom_event_prevent_default(event);
	dom_event_unref(event);

	return JS_UNDEFINED;
}

static JSValue
js_customEvent_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(new_target);

	JSValue obj = JS_NewObjectClass(ctx, js_customEvent_class_id);
	REF_JS(obj);

	if (JS_IsException(obj)) {
		return obj;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return JS_EXCEPTION;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		return JS_EXCEPTION;
	}
	dom_custom_event *event = NULL;
	dom_string *CustomEventStr = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)"CustomEvent", sizeof("CustomEvent") - 1, &CustomEventStr);

	if (exc != DOM_NO_ERR || !CustomEventStr) {
		return JS_EXCEPTION;
	}
	exc = dom_document_event_create_event(document->dom, CustomEventStr, &event);
	dom_string_unref(CustomEventStr);

	if (exc != DOM_NO_ERR) {
		return JS_EXCEPTION;
	}
	dom_string *typ = NULL;

	if (argc > 0) {
		size_t len;
		const char *tt = JS_ToCStringLen(ctx, &len, argv[0]);

		if (!tt) {
			return JS_EXCEPTION;
		}
		exc = dom_string_create((const uint8_t *)tt, strlen(tt), &typ);
		JS_FreeCString(ctx, tt);
	}
	bool bubbles = false;
	bool cancelable = false;

	JSValue *detail = NULL;

	if (argc > 1) {
		JSValue r = JS_GetPropertyStr(ctx, argv[1], "bubbles");
		bubbles = JS_ToBool(ctx, r);
		r = JS_GetPropertyStr(ctx, argv[1], "cancelable");
		cancelable = JS_ToBool(ctx, r);
		//r = JS_GetPropertyStr(ctx, argv[1], "composed");
		//composed = JS_ToBool(ctx, r);
		detail = calloc(1, sizeof(*detail));

		if (detail) {
			*detail = JS_GetPropertyStr(ctx, argv[1], "detail");
		}
	}
	exc = dom_custom_event_init_ns(event, NULL, typ, bubbles, cancelable, detail);

	if (typ) {
		dom_string_unref(typ);
	}
	JS_SetOpaque(obj, event);

	return obj;
}

static void
JS_NewGlobalCConstructor2(JSContext *ctx, JSValue func_obj, const char *name, JSValueConst proto)
{
	REF_JS(func_obj);
	REF_JS(proto);

	JSValue global_object = JS_GetGlobalObject(ctx);

	JS_DefinePropertyValueStr(ctx, global_object, name,
		JS_DupValue(ctx, func_obj), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	JS_SetConstructor(ctx, func_obj, proto);
	JS_FreeValue(ctx, func_obj);
	JS_FreeValue(ctx, global_object);
}

static JSValueConst
JS_NewGlobalCConstructor(JSContext *ctx, const char *name, JSCFunction *func, int length, JSValueConst proto)
{
	JSValue func_obj;
	func_obj = JS_NewCFunction2(ctx, func, name, length, JS_CFUNC_constructor_or_func, 0);
	REF_JS(func_obj);
	REF_JS(proto);

	JS_NewGlobalCConstructor2(ctx, func_obj, name, proto);

	return func_obj;
}

int
js_customEvent_init(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto;

	/* Event class */
	JS_NewClassID(&js_customEvent_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_customEvent_class_id, &js_customEvent_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_customEvent_proto_funcs, countof(js_customEvent_proto_funcs));
	JS_SetClassProto(ctx, js_customEvent_class_id, proto);

	/* Event object */
	(void)JS_NewGlobalCConstructor(ctx, "CustomEvent", js_customEvent_constructor, 1, proto);
	//REF_JS(obj);

//	JS_SetPropertyFunctionList(ctx, obj, js_customEvent_class_funcs, countof(js_customEvent_class_funcs));

	return 0;
}
