#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBodyElement.h"
#include "dom/node.h"

static JSBool
HTMLBodyElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct BODY_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLBodyElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BODY_ELEMENT_ALINK:
		string_to_jsval(ctx, vp, html->alink);
		break;
	case JSP_HTML_BODY_ELEMENT_BACKGROUND:
		string_to_jsval(ctx, vp, html->background);
		break;
	case JSP_HTML_BODY_ELEMENT_BGCOLOR:
		string_to_jsval(ctx, vp, html->bgcolor);
		break;
	case JSP_HTML_BODY_ELEMENT_LINK:
		string_to_jsval(ctx, vp, html->link);
		break;
	case JSP_HTML_BODY_ELEMENT_TEXT:
		string_to_jsval(ctx, vp, html->text);
		break;
	case JSP_HTML_BODY_ELEMENT_VLINK:
		string_to_jsval(ctx, vp, html->vlink);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLBodyElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct BODY_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLBodyElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BODY_ELEMENT_ALINK:
		mem_free_set(&html->alink, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BODY_ELEMENT_BACKGROUND:
		mem_free_set(&html->background, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BODY_ELEMENT_BGCOLOR:
		mem_free_set(&html->bgcolor, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BODY_ELEMENT_LINK:
		mem_free_set(&html->link, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BODY_ELEMENT_TEXT:
		mem_free_set(&html->text, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BODY_ELEMENT_VLINK:
		mem_free_set(&html->vlink, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLBodyElement_props[] = {
	{ "aLink",	JSP_HTML_BODY_ELEMENT_ALINK,		JSPROP_ENUMERATE },
	{ "background",	JSP_HTML_BODY_ELEMENT_BACKGROUND,	JSPROP_ENUMERATE },
	{ "bgColor",	JSP_HTML_BODY_ELEMENT_BGCOLOR,		JSPROP_ENUMERATE },
	{ "link",	JSP_HTML_BODY_ELEMENT_LINK,		JSPROP_ENUMERATE },
	{ "text",	JSP_HTML_BODY_ELEMENT_TEXT,		JSPROP_ENUMERATE },
	{ "vLink",	JSP_HTML_BODY_ELEMENT_VLINK,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLBodyElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLBodyElement_class = {
	"HTMLBodyElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLBodyElement_getProperty, HTMLBodyElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};
