#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFrameElement.h"
#include "dom/node.h"

static JSBool
HTMLFrameElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct FRAME_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLFrameElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FRAME_ELEMENT_FRAME_BORDER:
		string_to_jsval(ctx, vp, html->frame_border);
		break;
	case JSP_HTML_FRAME_ELEMENT_LONG_DESC:
		string_to_jsval(ctx, vp, html->long_desc);
		break;
	case JSP_HTML_FRAME_ELEMENT_MARGIN_HEIGHT:
		string_to_jsval(ctx, vp, html->margin_height);
		break;
	case JSP_HTML_FRAME_ELEMENT_MARGIN_WIDTH:
		string_to_jsval(ctx, vp, html->margin_width);
		break;
	case JSP_HTML_FRAME_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_FRAME_ELEMENT_NO_RESIZE:
		boolean_to_jsval(ctx, vp, html->no_resize);
		break;
	case JSP_HTML_FRAME_ELEMENT_SCROLLING:
		string_to_jsval(ctx, vp, html->scrolling);
		break;
	case JSP_HTML_FRAME_ELEMENT_SRC:
		string_to_jsval(ctx, vp, html->src);
		break;
	case JSP_HTML_FRAME_ELEMENT_CONTENT_DOCUMENT:
		string_to_jsval(ctx, vp, html->content_document);
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLFrameElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct FRAME_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLFrameElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FRAME_ELEMENT_FRAME_BORDER:
		mem_free_set(&html->frame_border, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FRAME_ELEMENT_LONG_DESC:
		mem_free_set(&html->long_desc, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FRAME_ELEMENT_MARGIN_HEIGHT:
		mem_free_set(&html->margin_height, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FRAME_ELEMENT_MARGIN_WIDTH:
		mem_free_set(&html->margin_width, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FRAME_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FRAME_ELEMENT_NO_RESIZE:
		html->no_resize = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_FRAME_ELEMENT_SCROLLING:
		mem_free_set(&html->scrolling, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FRAME_ELEMENT_SRC:
		mem_free_set(&html->src, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLFrameElement_props[] = {
	{ "frameBorder",	JSP_HTML_FRAME_ELEMENT_FRAME_BORDER,	JSPROP_ENUMERATE },
	{ "longDesc",		JSP_HTML_FRAME_ELEMENT_LONG_DESC,	JSPROP_ENUMERATE },
	{ "marginHeight",	JSP_HTML_FRAME_ELEMENT_MARGIN_HEIGHT,	JSPROP_ENUMERATE },
	{ "marginWidth",	JSP_HTML_FRAME_ELEMENT_MARGIN_WIDTH,	JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_FRAME_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "noResize",		JSP_HTML_FRAME_ELEMENT_NO_RESIZE,	JSPROP_ENUMERATE },
	{ "scrolling",		JSP_HTML_FRAME_ELEMENT_SCROLLING,	JSPROP_ENUMERATE },
	{ "src",		JSP_HTML_FRAME_ELEMENT_SRC,		JSPROP_ENUMERATE },
	{ "contentDocument",	JSP_HTML_FRAME_ELEMENT_CONTENT_DOCUMENT,JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLFrameElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLFrameElement_class = {
	"HTMLFrameElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLFrameElement_getProperty, HTMLFrameElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_FRAME_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct FRAME_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLFrameElement_class, o->HTMLElement_object, NULL);
	}
}

