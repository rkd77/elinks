#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBaseFontElement.h"
#include "dom/node.h"

static JSBool
HTMLBaseFontElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct BASEFONT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLBaseFontElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BASE_FONT_ELEMENT_COLOR:
		string_to_jsval(ctx, vp, html->color);
		break;
	case JSP_HTML_BASE_FONT_ELEMENT_FACE:
		string_to_jsval(ctx, vp, html->face);
		break;
	case JSP_HTML_BASE_FONT_ELEMENT_SIZE:
		int_to_jsval(ctx, vp, html->size);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLBaseFontElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct BASEFONT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLBaseFontElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BASE_FONT_ELEMENT_COLOR:
		mem_free_set(&html->color, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BASE_FONT_ELEMENT_FACE:
		mem_free_set(&html->face, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BASE_FONT_ELEMENT_SIZE:
		return JS_ValueToInt32(ctx, *vp, &html->size);
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLBaseFontElement_props[] = {
	{ "color",	JSP_HTML_BASE_FONT_ELEMENT_COLOR,	JSPROP_ENUMERATE },
	{ "face",	JSP_HTML_BASE_FONT_ELEMENT_FACE,	JSPROP_ENUMERATE },
	{ "size",	JSP_HTML_BASE_FONT_ELEMENT_SIZE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLBaseFontElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLBaseFontElement_class = {
	"HTMLBaseFontElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLBaseFontElement_getProperty, HTMLBaseFontElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};
