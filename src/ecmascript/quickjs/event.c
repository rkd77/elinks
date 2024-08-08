/* The QuickJS Event object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/dom.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/element.h"
#include "ecmascript/quickjs/event.h"
#include "intl/charsets.h"
#include "terminal/event.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

JSClassID js_event_class_id;

static JSValue js_event_get_property_bubbles(JSContext *ctx, JSValueConst this_val);
static JSValue js_event_get_property_cancelable(JSContext *ctx, JSValueConst this_val);
//static JSValue js_event_get_property_composed(JSContext *ctx, JSValueConst this_val);
static JSValue js_event_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val);
static JSValue js_event_get_property_target(JSContext *ctx, JSValueConst this_val);
static JSValue js_event_get_property_type(JSContext *ctx, JSValueConst this_val);

static JSValue js_event_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

static
void js_event_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	dom_event *event = (dom_event *)JS_GetOpaque(val, js_event_class_id);

	if (event) {
		dom_event_unref(event);
	}
}

static JSClassDef js_event_class = {
	"Event",
	js_event_finalizer
};

static const JSCFunctionListEntry js_event_proto_funcs[] = {
	JS_CGETSET_DEF("bubbles",	js_event_get_property_bubbles, NULL),
	JS_CGETSET_DEF("cancelable",	js_event_get_property_cancelable, NULL),
//	JS_CGETSET_DEF("composed",	js_event_get_property_composed, NULL),
	JS_CGETSET_DEF("defaultPrevented",	js_event_get_property_defaultPrevented, NULL),
	JS_CGETSET_DEF("target",	js_event_get_property_target, NULL),
	JS_CGETSET_DEF("type",	js_event_get_property_type, NULL),
	JS_CFUNC_DEF("preventDefault", 0, js_event_preventDefault),
};

static JSValue
js_event_get_property_bubbles(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_event *event = (dom_event *)(JS_GetOpaque(this_val, js_event_class_id));

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
js_event_get_property_cancelable(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_event *event = (dom_event *)(JS_GetOpaque(this_val, js_event_class_id));

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
js_event_get_property_composed(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_event *event = (struct eljs_event *)(JS_GetOpaque(this_val, js_event_class_id));

	if (!event) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, event->composed);

	RETURN_JS(r);
}
#endif

static JSValue
js_event_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_event *event = (dom_event *)(JS_GetOpaque(this_val, js_event_class_id));

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
js_event_get_property_target(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_event *event = (dom_event *)(JS_GetOpaque(this_val, js_event_class_id));

	if (!event) {
		return JS_NULL;
	}
	//dom_event_ref(event);
	dom_event_target *target = NULL;
	dom_exception exc = dom_event_get_target(event, &target);

	if (exc != DOM_NO_ERR || !target) {
		return JS_NULL;
	}
	JSValue r = getElement(ctx, target);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(target);
	//dom_event_unref(event);

	RETURN_JS(r);
}

static JSValue
js_event_get_property_type(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_event *event = (dom_event *)(JS_GetOpaque(this_val, js_event_class_id));

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
js_event_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_event *event = (dom_event *)(JS_GetOpaque(this_val, js_event_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_event_ref(event);
	dom_event_prevent_default(event);
	dom_event_unref(event);

	return JS_UNDEFINED;
}

static JSValue
js_event_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(new_target);

	JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
	REF_JS(obj);

	if (JS_IsException(obj)) {
		return obj;
	}
	dom_event *event = NULL;
	dom_exception exc = dom_event_create(&event);

	if (exc != DOM_NO_ERR) {
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}
	dom_string *typ = NULL;

	if (argc > 0) {
		const char *str;
		size_t len;

		str = JS_ToCStringLen(ctx, &len, argv[0]);

		if (str) {
			exc = dom_string_create((const uint8_t *)str, len, &typ);
			JS_FreeCString(ctx, str);
		}
	}
	bool bubbles = false;
	bool cancelable = false;

	if (argc > 1) {
		JSValue r = JS_GetPropertyStr(ctx, argv[1], "bubbles");
		bubbles = JS_ToBool(ctx, r);
		r = JS_GetPropertyStr(ctx, argv[1], "cancelable");
		cancelable = JS_ToBool(ctx, r);
//		r = JS_GetPropertyStr(ctx, argv[1], "composed");
//		event->composed = JS_ToBool(ctx, r);
	}
	exc = dom_event_init(event, typ, bubbles, cancelable);

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
js_event_init(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto;

	/* Event class */
	JS_NewClassID(&js_event_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_event_class_id, &js_event_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_event_proto_funcs, countof(js_event_proto_funcs));
	JS_SetClassProto(ctx, js_event_class_id, proto);

	/* Event object */
	(void)JS_NewGlobalCConstructor(ctx, "Event", js_event_constructor, 1, proto);
	//REF_JS(obj);

//	JS_SetPropertyFunctionList(ctx, obj, js_event_class_funcs, countof(js_event_class_funcs));

	return 0;
}

JSValue
getEvent(JSContext *ctx, void *eve)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue obj = JS_NewObjectClass(ctx, js_event_class_id);
	REF_JS(obj);

	dom_event *event = (dom_event *)eve;
	dom_event_ref(event);
	JS_SetOpaque(obj, event);
	JSValue r = obj;

	RETURN_JS(r);
}
