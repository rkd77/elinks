#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOListElement.h"
#include "dom/node.h"

static JSBool
HTMLOListElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct OL_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLOListElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OLIST_ELEMENT_COMPACT:
		boolean_to_jsval(ctx, vp, html->compact);
		break;
	case JSP_HTML_OLIST_ELEMENT_START:
		int_to_jsval(ctx, vp, html->start);
		break;
	case JSP_HTML_OLIST_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLOListElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct OL_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLOListElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OLIST_ELEMENT_COMPACT:
		html->compact = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_OLIST_ELEMENT_START:
		return JS_ValueToInt32(ctx, *vp, &html->start);
	case JSP_HTML_OLIST_ELEMENT_TYPE:
		mem_free_set(&html->type, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLOListElement_props[] = {
	{ "compact",	JSP_HTML_OLIST_ELEMENT_COMPACT,	JSPROP_ENUMERATE },
	{ "start",	JSP_HTML_OLIST_ELEMENT_START,	JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_OLIST_ELEMENT_TYPE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLOListElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLOListElement_class = {
	"HTMLOListElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLOListElement_getProperty, HTMLOListElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};
