/* The QuickJS KeyboardEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/keyboard.h"
#include "intl/charsets.h"
#include "terminal/event.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_keyboardEvent_class_id;

static JSValue js_keyboardEvent_get_property_key(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_keyCode(JSContext *ctx, JSValueConst this_val);

static JSValue js_keyboardEvent_get_property_bubbles(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_cancelable(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_composed(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val);
static JSValue js_keyboardEvent_get_property_type(JSContext *ctx, JSValueConst this_val);

static JSValue js_keyboardEvent_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);


static unicode_val_T keyCode;

struct keyboard {
	unicode_val_T keyCode;
	char *type_;
	unsigned int bubbles:1;
	unsigned int cancelable:1;
	unsigned int composed:1;
	unsigned int defaultPrevented:1;
};

static
void js_keyboardEvent_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	struct keyboard *keyb = (struct keyboard *)JS_GetOpaque(val, js_keyboardEvent_class_id);

	if (keyb) {
		mem_free_if(keyb->type_);
		mem_free(keyb);
	}
}

static JSClassDef js_keyboardEvent_class = {
	"KeyboardEvent",
	js_keyboardEvent_finalizer
};

static const JSCFunctionListEntry js_keyboardEvent_proto_funcs[] = {
	JS_CGETSET_DEF("bubbles",	js_keyboardEvent_get_property_bubbles, NULL),
	JS_CGETSET_DEF("cancelable",	js_keyboardEvent_get_property_cancelable, NULL),
	JS_CGETSET_DEF("composed",	js_keyboardEvent_get_property_composed, NULL),
	JS_CGETSET_DEF("defaultPrevented",	js_keyboardEvent_get_property_defaultPrevented, NULL),
	JS_CGETSET_DEF("key",	js_keyboardEvent_get_property_key, NULL),
	JS_CGETSET_DEF("keyCode",	js_keyboardEvent_get_property_keyCode, NULL),
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

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, keyb->bubbles);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_get_property_cancelable(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, keyb->cancelable);

	RETURN_JS(r);
}

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

static JSValue
js_keyboardEvent_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, keyb->defaultPrevented);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_get_property_key(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	char text[8] = {0};

	*text = keyb->keyCode;
	JSValue r = JS_NewString(ctx, text);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_get_property_keyCode(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	return JS_NewUint32(ctx, keyb->keyCode);
}

static JSValue
js_keyboardEvent_get_property_type(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	if (!keyb->type_) {
		JSValue r = JS_NewString(ctx, "");
		RETURN_JS(r);
	}
	JSValue r = JS_NewString(ctx, keyb->type_);

	RETURN_JS(r);
}

static JSValue
js_keyboardEvent_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct keyboard *keyb = (struct keyboard *)(JS_GetOpaque(this_val, js_keyboardEvent_class_id));

	if (!keyb) {
		return JS_NULL;
	}
	if (keyb->cancelable) {
		keyb->defaultPrevented = 1;
	}
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
	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		return JS_NULL;
	}
	keyCode = get_kbd_key(ev);

	if (keyCode == KBD_ENTER) {
		keyCode = 13;
	}
	keyb->keyCode = keyCode;

	JSValue keyb_obj = JS_NewObjectClass(ctx, js_keyboardEvent_class_id);
	JS_SetPropertyFunctionList(ctx, keyb_obj, js_keyboardEvent_proto_funcs, countof(js_keyboardEvent_proto_funcs));
	JS_SetClassProto(ctx, js_keyboardEvent_class_id, keyb_obj);
	JS_SetOpaque(keyb_obj, keyb);

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
	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		return JS_NULL;
	}

	if (argc > 0) {
		const char *str;
		size_t len;

		str = JS_ToCStringLen(ctx, &len, argv[0]);

		if (str) {
			keyb->type_ = memacpy(str, len);
			JS_FreeCString(ctx, str);
		}
	}

	if (argc > 1) {
		JSValue r = JS_GetPropertyStr(ctx, argv[1], "bubbles");
		keyb->bubbles = JS_ToBool(ctx, r);
		r = JS_GetPropertyStr(ctx, argv[1], "cancelable");
		keyb->cancelable = JS_ToBool(ctx, r);
		r = JS_GetPropertyStr(ctx, argv[1], "composed");
		keyb->composed = JS_ToBool(ctx, r);
		r = JS_GetPropertyStr(ctx, argv[1], "keyCode");
		int keyCode;
		if (JS_ToInt32(ctx, &keyCode, argv[1])) {
			keyb->keyCode = keyCode;
		}
	}
	JS_SetOpaque(obj, keyb);

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
