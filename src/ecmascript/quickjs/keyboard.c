/* The QuickJS KeyboardEvent object implementation. */

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
#include "ecmascript/quickjs/keyboard.h"
#include "intl/charsets.h"
#include "terminal/event.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_keyboardEvent_class_id;

static JSValue js_keyboardEvent_get_property_code(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_key(JSContext *ctx, JSValueConst this_val);

static JSValue js_keyboardEvent_get_property_bubbles(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_cancelable(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_composed(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_type(JSContext *ctx, JSValueConst this_val);

static JSValue js_keyboardEvent_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

static unicode_val_T keyCode;

static
void js_keyboardEvent_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	dom_keyboard_event *event = (dom_keyboard_event *)JS_GetOpaque(val, js_keyboardEvent_class_id);

	if (event) {
		dom_event_unref(event);
	}
}

static JSClassDef js_keyboardEvent_class = {
	"KeyboardEvent",
	js_keyboardEvent_finalizer
};

static const JSCFunctionListEntry js_keyboardEvent_proto_funcs[] = {
	JS_CGETSET_DEF("bubbles",	js_keyboardEvent_get_property_bubbles, NULL),
	JS_CGETSET_DEF("cancelable",	js_keyboardEvent_get_property_cancelable, NULL),
	JS_CGETSET_DEF("code",	js_keyboardEvent_get_property_code, NULL),
//	JS_CGETSET_DEF("composed",	js_keyboardEvent_get_property_composed, NULL),
	JS_CGETSET_DEF("defaultPrevented",	js_keyboardEvent_get_property_defaultPrevented, NULL),
	JS_CGETSET_DEF("key",	js_keyboardEvent_get_property_key, NULL),
	JS_CGETSET_DEF("type",	js_keyboardEvent_get_property_type, NULL),
	JS_CFUNC_DEF("preventDefault", 0, js_keyboardEvent_preventDefault)
};

static JSValue
js_keyboardEvent_get_property_bubbles(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_keyboard_event *event = (dom_keyboard_event *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	bool bubbles = false;
	dom_exception exc = dom_event_get_bubbles(event, &bubbles);
	JSValue r = JS_NewBool(ctx, bubbles);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_get_property_cancelable(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_keyboard_event *event = (dom_keyboard_event *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	bool cancelable = false;
	dom_exception exc = dom_event_get_cancelable(event, &cancelable);
	JSValue r = JS_NewBool(ctx, cancelable);

	RETURN_JS(r);
}
#if 0
static JSValue
js_keyboardEvent_get_property_composed(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, keyb->composed);

	RETURN_JS(r);
}
#endif

static JSValue
js_keyboardEvent_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_keyboard_event *event = (dom_keyboard_event *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	bool prevented = false;
	dom_exception exc = dom_event_is_default_prevented(event, &prevented);
	JSValue r = JS_NewBool(ctx, prevented);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_get_property_key(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct dom_keyboard_event *event = (dom_keyboard_event *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_string *key = NULL;
	dom_exception exc = dom_keyboard_event_get_key(event, &key);

	if (exc != DOM_NO_ERR || !key) {
		return JS_NULL;
	}
	JSValue r = JS_NewString(ctx, dom_string_data(key));
	dom_string_unref(key);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_get_property_code(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct dom_keyboard_event *event = (dom_keyboard_event *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_string *code = NULL;
	dom_exception exc = dom_keyboard_event_get_code(event, &code);

	if (exc != DOM_NO_ERR || !code) {
		return JS_NULL;
	}
	JSValue r = JS_NewString(ctx, dom_string_data(code));
	dom_string_unref(code);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_get_property_type(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct dom_keyboard_event *event = (dom_keyboard_event *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		JSValue r = JS_NewString(ctx, "");
		RETURN_JS(r);
	}
	JSValue r = JS_NewString(ctx, dom_string_data(typ));
	dom_string_unref(typ);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_keyboard_event *event = (dom_keyboard_event *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	dom_event_prevent_default(event);

	return JS_UNDEFINED;
}

JSValue
get_keyboardEvent(JSContext *ctx, struct term_event *ev)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	static int initialized;
	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_keyboardEvent_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_keyboardEvent_class_id, &js_keyboardEvent_class);
		initialized = 1;
	}
	dom_keyboard_event *event = NULL;
	dom_exception exc = dom_keyboard_event_create(&event);

	if (exc != DOM_NO_ERR) {
		return JS_NULL;
	}
	keyCode = get_kbd_key(ev);

	if (keyCode == KBD_ENTER) {
		keyCode = 13;
	}
//	keyb->keyCode = keyCode;

	exc = dom_keyboard_event_init(event, NULL, false, false,
		NULL, NULL, NULL, DOM_KEY_LOCATION_STANDARD,
		false, false, false, false,
		false, false);

	JSValue keyb_obj = JS_NewObjectClass(ctx, js_keyboardEvent_class_id);
	JS_SetPropertyFunctionList(ctx, keyb_obj, js_keyboardEvent_proto_funcs, countof(js_keyboardEvent_proto_funcs));
	JS_SetClassProto(ctx, js_keyboardEvent_class_id, keyb_obj);
	JS_SetOpaque(keyb_obj, event);

	JSValue rr = JS_DupValue(ctx, keyb_obj);
	RETURN_JS(rr);
}

static JSValue
js_keyboardEvent_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(new_target);

	JSValue obj = JS_NewObjectClass(ctx, js_keyboardEvent_class_id);
	REF_JS(obj);

	if (JS_IsException(obj)) {
		return obj;
	}
	dom_keyboard_event *event = NULL;
	dom_exception exc = dom_keyboard_event_create(&event);

	if (exc != DOM_NO_ERR) {
		return JS_EXCEPTION;
	}
	dom_string *typ = NULL;
	size_t len;

	if (argc > 0) {
		char *t = JS_ToCStringLen(ctx, &len, argv[0]);

		if (t) {
			exc = dom_string_create(t, strlen(t), &typ);
			JS_FreeCString(ctx, t);
		}
	}
	bool bubbles = false;
	bool cancelable = false;
	dom_string *key = NULL;
	dom_string *code = NULL;

	if (argc > 1) {
		JSValue r = JS_GetPropertyStr(ctx, argv[1], "bubbles");
		bubbles = JS_ToBool(ctx, r);
		r = JS_GetPropertyStr(ctx, argv[1], "cancelable");
		cancelable = JS_ToBool(ctx, r);

		r = JS_GetPropertyStr(ctx, argv[1], "key");
		const char *k = JS_ToCStringLen(ctx, &len, r);

		if (k) {
			exc = dom_string_create(k, strlen(k), &key);
			JS_FreeCString(ctx, k);
			JS_FreeValue(ctx, r);
		}
		r = JS_GetPropertyStr(ctx, argv[1], "code");
		const char *c = JS_ToCStringLen(ctx, &len, r);

		if (c) {
			exc = dom_string_create(c, strlen(c), &code);
			JS_FreeCString(ctx, c);
			JS_FreeValue(ctx, r);
		}
	}
	exc = dom_keyboard_event_init(event, typ, bubbles, cancelable, NULL/*view*/,
		key, code, DOM_KEY_LOCATION_STANDARD,
		false, false, false,
		false, false, false);
	if (typ) dom_string_unref(typ);
	if (key) dom_string_unref(key);
	if (code) dom_string_unref(code);

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
js_keyboardEvent_init(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto, obj;

	/* Event class */
	JS_NewClassID(&js_keyboardEvent_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_keyboardEvent_class_id, &js_keyboardEvent_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_keyboardEvent_proto_funcs, countof(js_keyboardEvent_proto_funcs));
	JS_SetClassProto(ctx, js_keyboardEvent_class_id, proto);

	/* Event object */
	obj = JS_NewGlobalCConstructor(ctx, "KeyboardEvent", js_keyboardEvent_constructor, 1, proto);
	REF_JS(obj);

//	JS_SetPropertyFunctionList(ctx, obj, js_keyboardEvent_class_funcs, countof(js_keyboardEvent_class_funcs));

	return 0;
}
