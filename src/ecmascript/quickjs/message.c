/* The QuickJS MessageEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/message.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_messageEvent_class_id;

static JSValue js_messageEvent_get_property_data(JSContext *cx, JSValueConst this_val);
static JSValue js_messageEvent_get_property_lastEventId(JSContext *cx, JSValueConst this_val);
static JSValue js_messageEvent_get_property_origin(JSContext *cx, JSValueConst this_val);
static JSValue js_messageEvent_get_property_source(JSContext *cx, JSValueConst this_val);

static JSValue js_messageEvent_get_property_bubbles(JSContext *ctx, JSValueConst this_val);
static JSValue js_messageEvent_get_property_cancelable(JSContext *ctx, JSValueConst this_val);
static JSValue js_messageEvent_get_property_composed(JSContext *ctx, JSValueConst this_val);
static JSValue js_messageEvent_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val);
static JSValue js_messageEvent_get_property_type(JSContext *ctx, JSValueConst this_val);

static JSValue js_messageEvent_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

struct message_event {
	char *data;
	char *lastEventId;
	char *origin;
	char *source;

	char *type_;
	unsigned int bubbles:1;
	unsigned int cancelable:1;
	unsigned int composed:1;
	unsigned int defaultPrevented:1;
};

static
void js_messageEvent_finalizer(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct message_event *event = (struct message_event *)JS_GetOpaque(val, js_messageEvent_class_id);

	if (event) {
		mem_free_if(event->data);
		mem_free_if(event->lastEventId);
		mem_free_if(event->origin);
		mem_free_if(event->source);
		mem_free_if(event->type_);
		mem_free(event);
	}
}

static JSClassDef js_messageEvent_class = {
	"MessageEvent",
	js_messageEvent_finalizer
};

static const JSCFunctionListEntry js_messageEvent_proto_funcs[] = {
	JS_CGETSET_DEF("bubbles",	js_messageEvent_get_property_bubbles, NULL),
	JS_CGETSET_DEF("cancelable",	js_messageEvent_get_property_cancelable, NULL),
	JS_CGETSET_DEF("composed",	js_messageEvent_get_property_composed, NULL),
	JS_CGETSET_DEF("data",	js_messageEvent_get_property_data, NULL),
	JS_CGETSET_DEF("defaultPrevented",	js_messageEvent_get_property_defaultPrevented, NULL),
	JS_CGETSET_DEF("lastEventId",	js_messageEvent_get_property_lastEventId, NULL),
	JS_CGETSET_DEF("origin",	js_messageEvent_get_property_origin, NULL),
	JS_CGETSET_DEF("source",	js_messageEvent_get_property_source, NULL),
	JS_CGETSET_DEF("type",	js_messageEvent_get_property_type, NULL),
	JS_CFUNC_DEF("preventDefault", 0, js_messageEvent_preventDefault),
};

static JSValue
js_messageEvent_get_property_bubbles(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, event->bubbles);

	RETURN_JS(r);
}

static JSValue
js_messageEvent_get_property_cancelable(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, event->cancelable);

	RETURN_JS(r);
}

static JSValue
js_messageEvent_get_property_composed(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, event->composed);

	RETURN_JS(r);
}

static JSValue
js_messageEvent_get_property_data(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

	if (!event || !event->data) {
		return JS_NULL;
	}
	JSValue r = JS_NewString(ctx, event->data);

	RETURN_JS(r);
}

static JSValue
js_messageEvent_get_property_defaultPrevented(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	JSValue r = JS_NewBool(ctx, event->defaultPrevented);

	RETURN_JS(r);
}

static JSValue
js_messageEvent_get_property_lastEventId(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

	if (!event || !event->lastEventId) {
		return JS_NULL;
	}
	JSValue r = JS_NewString(ctx, event->lastEventId);

	RETURN_JS(r);
}

static JSValue
js_messageEvent_get_property_origin(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

	if (!event || !event->origin) {
		return JS_NULL;
	}
	JSValue r = JS_NewString(ctx, event->origin);

	RETURN_JS(r);
}

static JSValue
js_messageEvent_get_property_source(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

	if (!event || !event->source) {
		return JS_NULL;
	}
	JSValue r = JS_NewString(ctx, event->source);

	RETURN_JS(r);
}

static JSValue
js_messageEvent_get_property_type(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

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
js_messageEvent_preventDefault(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct message_event *event = (struct message_event *)(JS_GetOpaque(this_val, js_messageEvent_class_id));

	if (!event) {
		return JS_NULL;
	}
	if (event->cancelable) {
		event->defaultPrevented = 1;
	}
	return JS_UNDEFINED;
}

static int lastEventId;

JSValue
get_messageEvent(JSContext *ctx, char *data, char *origin, char *source)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	static int initialized;
	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_messageEvent_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_messageEvent_class_id, &js_messageEvent_class);
		initialized = 1;
	}
	struct message_event *event = (struct message_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		return JS_NULL;
	}
	event->data = null_or_stracpy(data);
	event->origin = null_or_stracpy(origin);
	event->source = null_or_stracpy(source);

	char id[32];

	snprintf(id, 31, "%d", ++lastEventId);
	event->lastEventId = stracpy(id);

	JSValue event_obj = JS_NewObjectClass(ctx, js_messageEvent_class_id);
	JS_SetPropertyFunctionList(ctx, event_obj, js_messageEvent_proto_funcs, countof(js_messageEvent_proto_funcs));
	JS_SetClassProto(ctx, js_messageEvent_class_id, event_obj);
	JS_SetOpaque(event_obj, event);

	JSValue rr = JS_DupValue(ctx, event_obj);
	RETURN_JS(rr);
}

static JSValue
js_messageEvent_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(new_target);

	JSValue obj = JS_NewObjectClass(ctx, js_messageEvent_class_id);
	REF_JS(obj);

	if (JS_IsException(obj)) {
		return obj;
	}
	struct message_event *event = (struct message_event *)mem_calloc(1, sizeof(*event));

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

	if (argc > 1) {
		const char *str;
		size_t len;

		JSValue r = JS_GetPropertyStr(ctx, argv[1], "bubbles");
		event->bubbles = JS_ToBool(ctx, r);
		r = JS_GetPropertyStr(ctx, argv[1], "cancelable");
		event->cancelable = JS_ToBool(ctx, r);
		r = JS_GetPropertyStr(ctx, argv[1], "composed");
		event->composed = JS_ToBool(ctx, r);

		r = JS_GetPropertyStr(ctx, argv[1], "data");
		str = JS_ToCStringLen(ctx, &len, r);

		if (str) {
			event->data = memacpy(str, len);
			JS_FreeCString(ctx, str);
		}
		r = JS_GetPropertyStr(ctx, argv[1], "lastEventId");
		str = JS_ToCStringLen(ctx, &len, r);

		if (str) {
			event->lastEventId = memacpy(str, len);
			JS_FreeCString(ctx, str);
		}
		r = JS_GetPropertyStr(ctx, argv[1], "origin");
		str = JS_ToCStringLen(ctx, &len, r);

		if (str) {
			event->origin = memacpy(str, len);
			JS_FreeCString(ctx, str);
		}
		r = JS_GetPropertyStr(ctx, argv[1], "source");
		str = JS_ToCStringLen(ctx, &len, r);

		if (str) {
			event->source = memacpy(str, len);
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
js_messageEvent_init(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto, obj;

	/* Event class */
	JS_NewClassID(&js_messageEvent_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_messageEvent_class_id, &js_messageEvent_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_messageEvent_proto_funcs, countof(js_messageEvent_proto_funcs));
	JS_SetClassProto(ctx, js_messageEvent_class_id, proto);

	/* Event object */
	obj = JS_NewGlobalCConstructor(ctx, "MessageEvent", js_messageEvent_constructor, 1, proto);
	REF_JS(obj);

//	JS_SetPropertyFunctionList(ctx, obj, js_messageEvent_class_funcs, countof(js_messageEvent_class_funcs));

	return 0;
}
