/* The QuickJS performance implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/ecmascript.h"
#include "js/quickjs.h"
#include "js/quickjs/performance.h"
#include "osdep/osdep.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

JSClassID js_performance_class_id;

static JSValue js_performance_get_property_timeOrigin(JSContext *ctx, JSValueConst this_val);

static
void js_performance_finalizer(JSRuntime *rt, JSValue val)
{
	ELOG
	REF_JS(val);
}

static JSClassDef js_performance_class = {
	"Performance",
	js_performance_finalizer
};

static JSValue
js_performance_now(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	JSValue r = JS_NewFloat64(ctx, get_time() - interpreter->time_origin);

	RETURN_JS(r);
}

static const JSCFunctionListEntry js_performance_proto_funcs[] = {
	JS_CGETSET_DEF("timeOrigin",	js_performance_get_property_timeOrigin, NULL),
	JS_CFUNC_DEF("now", 0, js_performance_now)
};

static JSValue
js_performance_get_property_timeOrigin(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	JSValue r = JS_NewFloat64(ctx, interpreter->time_origin);

	RETURN_JS(r);
}

JSValue
getPerformance(JSContext *ctx)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto;
	{
		static int initialised;

		if (!initialised) {
			/* Event class */
			JS_NewClassID(JS_GetRuntime(ctx), &js_performance_class_id);
			JS_NewClass(JS_GetRuntime(ctx), js_performance_class_id, &js_performance_class);
			initialised = 1;
		}
	}
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_performance_proto_funcs, countof(js_performance_proto_funcs));
	JS_SetClassProto(ctx, js_performance_class_id, proto);
	JSValue r = proto;

	JS_DupValue(ctx, r);

	RETURN_JS(r);
}
