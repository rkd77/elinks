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
#include "ecmascript/quickjs/mapa.h"
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
	char *res = NULL;
	JSValue r;

	if (!el) {
		return JS_NULL;
	}
	dom_node_ref(el);
	exc = dom_element_get_attribute(el, corestring_dom_style, &style);

	if (exc != DOM_NO_ERR) {
		r = JS_NewString(ctx, "");
		dom_node_unref(el);
		RETURN_JS(r);
	}

	if (!style || !dom_string_length(style)) {
		r = JS_NewString(ctx, "");

		if (style) {
			dom_string_unref(style);
		}
		dom_node_unref(el);
		RETURN_JS(r);
	}

	res = get_css_value(dom_string_data(style), property);
	dom_string_unref(style);

	if (!res) {
		r = JS_NewString(ctx, "");
		dom_node_unref(el);
		RETURN_JS(r);
	}
	r = JS_NewString(ctx, res);
	mem_free(res);
	dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_style_get_property_cssText(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(JS_GetOpaque(this_val, js_style_class_id));
	dom_exception exc;
	dom_string *style = NULL;
	JSValue r;

	if (!el) {
		return JS_NULL;
	}
	dom_node_ref(el);
	exc = dom_element_get_attribute(el, corestring_dom_style, &style);

	if (exc != DOM_NO_ERR || !style) {
		r = JS_NewString(ctx, "");
		dom_node_unref(el);

		RETURN_JS(r);
	}
	r = JS_NewString(ctx, dom_string_data(style));
	dom_string_unref(style);
	dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_set_style(JSContext *ctx, JSValueConst this_val, JSValue val, const char *property)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *el = (dom_node *)(JS_GetOpaque(this_val, js_style_class_id));
	dom_string *style = NULL;
	dom_string *stylestr = NULL;
	char *res = NULL;
	const char *value;
	size_t len;

	if (!el) {
		return JS_NULL;
	}
	dom_node_ref(el);
	value = JS_ToCStringLen(ctx, &len, val);

	if (!value) {
		dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_element_get_attribute(el, corestring_dom_style, &style);

	if (exc != DOM_NO_ERR) {
		dom_node_unref(el);
		return JS_UNDEFINED;
	}

	if (!style || !dom_string_length(style)) {
		res = set_css_value("", property, value);

		if (style) {
			dom_string_unref(style);
		}
	} else {
		res = set_css_value(dom_string_data(style), property, value);
		dom_string_unref(style);
	}
	JS_FreeCString(ctx, value);
	exc = dom_string_create((const uint8_t *)res, strlen(res), &stylestr);

	if (exc == DOM_NO_ERR && stylestr) {
		exc = dom_element_set_attribute(el, corestring_dom_style, stylestr);
		interpreter->changed = 1;
		dom_string_unref(stylestr);
	}
	mem_free(res);
	dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_style_set_property_cssText(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *el = (dom_node *)(JS_GetOpaque(this_val, js_style_class_id));
	dom_exception exc;
	char *res = NULL;

	if (!el) {
		return JS_UNDEFINED;
	}
	dom_node_ref(el);
	const char *value;
	size_t len;

	if (!el) {
		dom_node_unref(el);
		return JS_NULL;
	}
	value = JS_ToCStringLen(ctx, &len, val);

	if (!value) {
		dom_node_unref(el);
		return JS_EXCEPTION;
	}
	void *css = set_elstyle(value);
	JS_FreeCString(ctx, value);

	if (!css) {
		dom_node_unref(el);
		return JS_UNDEFINED;
	}
	res = get_elstyle(css);

	if (!res) {
		dom_node_unref(el);
		return JS_UNDEFINED;
	}
	dom_string *stylestr = NULL;
	exc = dom_string_create((const uint8_t *)res, strlen(res), &stylestr);

	if (exc == DOM_NO_ERR && stylestr) {
		exc = dom_element_set_attribute(el, corestring_dom_style, stylestr);
		interpreter->changed = 1;
		dom_string_unref(stylestr);
	}
	mem_free(res);
	dom_node_unref(el);

	return JS_UNDEFINED;
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
js_style_get_property_backgroundClip(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "background-clip");
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
js_style_get_property_height(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "height");
}

static JSValue
js_style_get_property_left(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "left");
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
js_style_get_property_top(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "top");
}

static JSValue
js_style_get_property_whiteSpace(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_style(ctx, this_val, "white-space");
}

static JSValue
js_style_set_property_background(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "background");
}

static JSValue
js_style_set_property_backgroundClip(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "background-clip");
}

static JSValue
js_style_set_property_backgroundColor(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "background-color");
}

static JSValue
js_style_set_property_color(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "color");
}

static JSValue
js_style_set_property_display(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "display");
}

static JSValue
js_style_set_property_fontStyle(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "font-style");
}

static JSValue
js_style_set_property_fontWeight(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "font-weight");
}

static JSValue
js_style_set_property_height(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "height");
}

static JSValue
js_style_set_property_left(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "left");
}

static JSValue
js_style_set_property_lineStyle(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "line-style");
}

static JSValue
js_style_set_property_lineStyleType(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "line-style-type");
}

static JSValue
js_style_set_property_textAlign(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "text-align");
}

static JSValue
js_style_set_property_textDecoration(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "text-decoration");
}

static JSValue
js_style_set_property_top(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "top");
}

static JSValue
js_style_set_property_whiteSpace(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_set_style(ctx, this_val, val, "white-space");
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
	JS_CGETSET_DEF("background", js_style_get_property_background, js_style_set_property_background),
	JS_CGETSET_DEF("backgroundClip", js_style_get_property_backgroundClip, js_style_set_property_backgroundClip),
	JS_CGETSET_DEF("backgroundColor", js_style_get_property_backgroundColor, js_style_set_property_backgroundColor),
	JS_CGETSET_DEF("color", js_style_get_property_color, js_style_set_property_color),
	JS_CGETSET_DEF("cssText", js_style_get_property_cssText, js_style_set_property_cssText),
	JS_CGETSET_DEF("display", js_style_get_property_display, js_style_set_property_display),
	JS_CGETSET_DEF("fontStyle", js_style_get_property_fontStyle, js_style_set_property_fontStyle),
	JS_CGETSET_DEF("fontWeight", js_style_get_property_fontWeight, js_style_set_property_fontWeight),
	JS_CGETSET_DEF("height", js_style_get_property_height, js_style_set_property_height),
	JS_CGETSET_DEF("left", js_style_get_property_left, js_style_set_property_left),
	JS_CGETSET_DEF("lineStyle", js_style_get_property_lineStyle, js_style_set_property_lineStyle),
	JS_CGETSET_DEF("lineStyleType", js_style_get_property_lineStyleType, js_style_set_property_lineStyleType),
	JS_CGETSET_DEF("textAlign", js_style_get_property_textAlign, js_style_set_property_textAlign),
	JS_CGETSET_DEF("textDecoration", js_style_get_property_textDecoration, js_style_set_property_textDecoration),
	JS_CGETSET_DEF("top", js_style_get_property_top, js_style_set_property_top),
	JS_CGETSET_DEF("whiteSpace", js_style_get_property_whiteSpace, js_style_set_property_whiteSpace),
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
