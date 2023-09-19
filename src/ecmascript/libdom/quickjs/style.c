/* The QuickJS style object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "document/libdom/corestrings.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/quickjs/mapa.h"
#include "ecmascript/quickjs/style.h"
#include "ecmascript/quickjs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_style_class_id;

static JSValue
js_style(JSContext *ctx, JSValueConst this_val, const char *property)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_exception exc;
	dom_node *el = (dom_node *)(JS_GetOpaque(this_val, js_style_class_id));
	dom_string *style = NULL;
	const char *res = NULL;
	JSValue r;

	if (!el) {
		return JS_NULL;
	}
	exc = dom_element_get_attribute(el, corestring_dom_style, &style);

	if (exc != DOM_NO_ERR) {
		r = JS_NewString(ctx, "");
		RETURN_JS(r);
	}

	if (!style || !dom_string_length(style)) {
		r = JS_NewString(ctx, "");

		if (style) {
			dom_string_unref(style);
		}
		RETURN_JS(r);
	}

	res = get_css_value(dom_string_data(style), property);
	dom_string_unref(style);

	if (!res) {
		r = JS_NewString(ctx, "");
		RETURN_JS(r);
	}
	r = JS_NewString(ctx, res);
	mem_free(res);
	RETURN_JS(r);
}

static JSValue
js_style_get_property_background(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "background");
}

static JSValue
js_style_get_property_backgroundColor(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "background-color");
}

static JSValue
js_style_get_property_color(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "color");
}

static JSValue
js_style_get_property_display(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "display");
}

static JSValue
js_style_get_property_fontStyle(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "font-style");
}

static JSValue
js_style_get_property_fontWeight(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "font-weight");
}

static JSValue
js_style_get_property_lineStyle(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "line-style");
}

static JSValue
js_style_get_property_lineStyleType(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "line-style-type");
}

static JSValue
js_style_get_property_textAlign(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "text-align");
}

static JSValue
js_style_get_property_textDecoration(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "text-decoration");
}

static JSValue
js_style_get_property_whiteSpace(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "white-space");
}

void js_style_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);
}

static JSValue
js_style_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	return JS_NewString(ctx, "[style object]");
}

static const JSCFunctionListEntry js_style_proto_funcs[] = {
	JS_CGETSET_DEF("background", js_style_get_property_background, NULL),
	JS_CGETSET_DEF("backgroundColor", js_style_get_property_backgroundColor, NULL),
	JS_CGETSET_DEF("color", js_style_get_property_color, NULL),
	JS_CGETSET_DEF("display", js_style_get_property_display, NULL),
	JS_CGETSET_DEF("fontStyle", js_style_get_property_fontStyle, NULL),
	JS_CGETSET_DEF("fontWeight", js_style_get_property_fontWeight, NULL),
	JS_CGETSET_DEF("lineStyle", js_style_get_property_lineStyle, NULL),
	JS_CGETSET_DEF("lineStyleType", js_style_get_property_lineStyleType, NULL),
	JS_CGETSET_DEF("textAlign", js_style_get_property_textAlign, NULL),
	JS_CGETSET_DEF("textDecoration", js_style_get_property_textDecoration, NULL),
	JS_CGETSET_DEF("whiteSpace", js_style_get_property_whiteSpace, NULL),
	JS_CFUNC_DEF("toString", 0, js_style_toString)
};

static JSClassDef js_style_class = {
	"style",
	js_style_finalizer
};

JSValue
getStyle(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue second = JS_NULL;
	static int initialized;
	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_style_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_style_class_id, &js_style_class);
		initialized = 1;
	}
	JSValue style_obj = JS_NewObjectClass(ctx, js_style_class_id);

	JS_SetPropertyFunctionList(ctx, style_obj, js_style_proto_funcs, countof(js_style_proto_funcs));
	JS_SetClassProto(ctx, js_style_class_id, style_obj);
	JS_SetOpaque(style_obj, node);

	JSValue rr = JS_DupValue(ctx, style_obj);

	RETURN_JS(rr);
}
