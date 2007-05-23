#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLStyleElement.h"
#include "dom/node.h"

static JSBool
HTMLStyleElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct STYLE_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLStyleElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_STYLE_ELEMENT_DISABLED:
		string_to_jsval(ctx, vp, html->disabled);
		break;
	case JSP_HTML_STYLE_ELEMENT_MEDIA:
		string_to_jsval(ctx, vp, html->media);
		break;
	case JSP_HTML_STYLE_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLStyleElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct STYLE_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLStyleElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_STYLE_ELEMENT_DISABLED:
		mem_free_set(&html->disabled, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_STYLE_ELEMENT_MEDIA:
		mem_free_set(&html->media, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_STYLE_ELEMENT_TYPE:
		mem_free_set(&html->type, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLStyleElement_props[] = {
	{ "disabled",	JSP_HTML_STYLE_ELEMENT_DISABLED,JSPROP_ENUMERATE },
	{ "media",	JSP_HTML_STYLE_ELEMENT_MEDIA,	JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_STYLE_ELEMENT_TYPE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLStyleElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLStyleElement_class = {
	"HTMLStyleElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLStyleElement_getProperty, HTMLStyleElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};
