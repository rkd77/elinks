#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFrameSetElement.h"
#include "dom/node.h"

static JSBool
HTMLFrameSetElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct FRAMESET_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLFrameSetElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FRAME_SET_ELEMENT_COLS:
		string_to_jsval(ctx, vp, html->cols);
		break;
	case JSP_HTML_FRAME_SET_ELEMENT_ROWS:
		string_to_jsval(ctx, vp, html->rows);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLFrameSetElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct FRAMESET_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLFrameSetElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FRAME_SET_ELEMENT_COLS:
		mem_free_set(&html->cols, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FRAME_SET_ELEMENT_ROWS:
		mem_free_set(&html->rows, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLFrameSetElement_props[] = {
	{ "cols",	JSP_HTML_FRAME_SET_ELEMENT_COLS,	JSPROP_ENUMERATE },
	{ "rows",	JSP_HTML_FRAME_SET_ELEMENT_ROWS,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLFrameSetElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLFrameSetElement_class = {
	"HTMLFrameSetElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLFrameSetElement_getProperty, HTMLFrameSetElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};
