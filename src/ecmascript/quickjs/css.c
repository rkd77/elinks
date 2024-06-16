/* The QuickJS CSSStyleDeclaration object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "document/libdom/corestrings.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/css.h"
#include "ecmascript/quickjs/element.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

void *map_csses;
void *map_rev_csses;

static void *
js_CSSStyleDeclaration_GetOpaque(JSValueConst this_val)
{
	REF_JS(this_val);

	return attr_find_in_map_rev(map_rev_csses, this_val);
}

static void
js_CSSStyleDeclaration_SetOpaque(JSValueConst this_val, void *node)
{
	REF_JS(this_val);

	if (!node) {
		attr_erase_from_map_rev(map_rev_csses, this_val);
	} else {
		attr_save_in_map_rev(map_rev_csses, this_val, node);
	}
}

static JSValue
js_CSSStyleDeclaration_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewInt32(ctx, 3); // fake
}

static JSValue
js_CSSStyleDeclaration_getPropertyValue(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_UNDEFINED;
}

static JSValue
js_CSSStyleDeclaration_item2(JSContext *ctx, JSValueConst this_val, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "0"); // fake
}

static JSValue
js_CSSStyleDeclaration_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_UNDEFINED;
	}
	return JS_NewString(ctx, "0"); // fake
}

static JSValue
js_CSSStyleDeclaration_namedItem2(JSContext *ctx, JSValueConst this_val, const char *str)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	return JS_NewString(ctx, "0"); // fake
}

static JSValue
js_CSSStyleDeclaration_namedItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_UNDEFINED;
	}
	return JS_NewString(ctx, "0"); // fake
}

static void
js_CSSStyleDeclaration_set_items(JSContext *ctx, JSValue this_val, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	JS_DefinePropertyValueStr(ctx, this_val, "marginTop", JS_NewString(ctx, "0"), 0);
	JS_DefinePropertyValueStr(ctx, this_val, "marginLeft", JS_NewString(ctx, "0"), 0);
	JS_DefinePropertyValueStr(ctx, this_val, "marginRight", JS_NewString(ctx, "0"), 0);
}

static JSValue
js_CSSStyleDeclaration_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[CSSStyleDeclaration object]");
}

static const JSCFunctionListEntry js_CSSStyleDeclaration_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_CSSStyleDeclaration_get_property_length, NULL),
	JS_CFUNC_DEF("getPropertyValue", 1, js_CSSStyleDeclaration_getPropertyValue),
	JS_CFUNC_DEF("item", 1, js_CSSStyleDeclaration_item),
	JS_CFUNC_DEF("namedItem", 1, js_CSSStyleDeclaration_namedItem),
	JS_CFUNC_DEF("toString", 0, js_CSSStyleDeclaration_toString)
};

JSValue
getCSSStyleDeclaration(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

#if 0
	static int initialized;
	JSValue second;

	if (!initialized) {
		initialized = 1;
	}
	second = attr_find_in_map(map_csses, node);

	if (!JS_IsNull(second)) {
		JSValue r = JS_DupValue(ctx, second);

		RETURN_JS(r);
	}
#endif
	JSValue CSSStyleDeclaration_obj = JS_NewArray(ctx);
	JS_SetPropertyFunctionList(ctx, CSSStyleDeclaration_obj, js_CSSStyleDeclaration_proto_funcs, countof(js_CSSStyleDeclaration_proto_funcs));
	//js_CSSStyleDeclaration_SetOpaque(CSSStyleDeclaration_obj, node);
	js_CSSStyleDeclaration_set_items(ctx, CSSStyleDeclaration_obj, node);
	//attr_save_in_map(map_csses, node, CSSStyleDeclaration_obj);
	RETURN_JS(CSSStyleDeclaration_obj);

//	JSValue rr = JS_DupValue(ctx, CSSStyleDeclaration_obj);

//	RETURN_JS(rr);
}
