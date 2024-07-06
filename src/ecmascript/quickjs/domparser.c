/* The QuickJS DOMParser implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/libdom/doc.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/dom.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/document.h"
#include "ecmascript/quickjs/domparser.h"
#include "intl/charsets.h"
#include "terminal/event.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

JSClassID js_domparser_class_id;

static JSValue js_domparser_parseFromString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

static JSClassDef js_domparser_class = {
	"DOMParser",
};

static const JSCFunctionListEntry js_domparser_proto_funcs[] = {
	JS_CFUNC_DEF("parseFromString", 2, js_domparser_parseFromString),
};

static JSValue
js_domparser_parseFromString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_NULL;
	}
	dom_html_document *doc = document_parse_text("utf-8", str, len);
	JS_FreeCString(ctx, str);

	if (!doc) {
		return JS_NULL;
	}

	return getDocument2(ctx, doc);
}

static JSValue
js_domparser_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(new_target);

	JSValue obj = JS_NewObjectClass(ctx, js_domparser_class_id);
	REF_JS(obj);

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
js_domparser_init(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto, obj;

	/* DOMParser class */
	JS_NewClassID(&js_domparser_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_domparser_class_id, &js_domparser_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_domparser_proto_funcs, countof(js_domparser_proto_funcs));
	JS_SetClassProto(ctx, js_domparser_class_id, proto);

	/* DOMParser object */
	obj = JS_NewGlobalCConstructor(ctx, "DOMParser", js_domparser_constructor, 1, proto);
	REF_JS(obj);

	return 0;
}
