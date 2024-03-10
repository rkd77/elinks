/* The QuickJS Event object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/event.h"
#include "intl/charsets.h"
#include "terminal/event.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_event_class_id;

static JSValue js_event_get_property_type(JSContext *ctx, JSValueConst this_val);

struct eljs_event {
	char *type_;
};

static
void js_event_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	struct eljs_event *event = (struct eljs_event *)JS_GetOpaque(val, js_event_class_id);

	if (event) {
		mem_free_if(event->type_);
		mem_free(event);
	}
}

static JSClassDef js_event_class = {
	"Event",
	js_event_finalizer
};

static const JSCFunctionListEntry js_event_proto_funcs[] = {
	JS_CGETSET_DEF("type",	js_event_get_property_type, NULL)
};

static JSValue
js_event_get_property_type(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_event *event = (struct eljs_event *)(JS_GetOpaque(this_val, js_event_class_id));

	if (!event) {
		return JS_NULL;
	}
	if (!event->type_) {
		JSValue r = JS_NewString(ctx, "");
		RETURN_JS(r);
	}
	JSValue r = JS_NewString(ctx, event->type_);

	RETURN_JS(r);
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
	struct eljs_event *event = (struct eljs_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}

	if (argc > 0) {
		const char *str;
		size_t len;

		str = JS_ToCStringLen(ctx, &len, argv[0]);

		if (str) {
			event->type_ = memacpy(str, len);
			JS_FreeCString(ctx, str);
		}
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
	JSValue proto, obj;

	/* Event class */
	JS_NewClassID(&js_event_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_event_class_id, &js_event_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_event_proto_funcs, countof(js_event_proto_funcs));
	JS_SetClassProto(ctx, js_event_class_id, proto);

	/* Event object */
	obj = JS_NewGlobalCConstructor(ctx, "Event", js_event_constructor, 1, proto);
	REF_JS(obj);

//	JS_SetPropertyFunctionList(ctx, obj, js_event_class_funcs, countof(js_event_class_funcs));

	return 0;
}
