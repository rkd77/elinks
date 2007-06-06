#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLIFrameElement.h"
#include "dom/node.h"

static JSBool
HTMLIFrameElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct IFRAME_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLIFrameElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IFRAME_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_IFRAME_ELEMENT_FRAME_BORDER:
		string_to_jsval(ctx, vp, html->frame_border);
		break;
	case JSP_HTML_IFRAME_ELEMENT_HEIGHT:
		string_to_jsval(ctx, vp, html->height);
		break;
	case JSP_HTML_IFRAME_ELEMENT_LONG_DESC:
		string_to_jsval(ctx, vp, html->long_desc);
		break;
	case JSP_HTML_IFRAME_ELEMENT_MARGIN_HEIGHT:
		string_to_jsval(ctx, vp, html->margin_height);
		break;
	case JSP_HTML_IFRAME_ELEMENT_MARGIN_WIDTH:
		string_to_jsval(ctx, vp, html->margin_width);
		break;
	case JSP_HTML_IFRAME_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_IFRAME_ELEMENT_SCROLLING:
		string_to_jsval(ctx, vp, html->scrolling);
		break;
	case JSP_HTML_IFRAME_ELEMENT_SRC:
		string_to_jsval(ctx, vp, html->src);
		break;
	case JSP_HTML_IFRAME_ELEMENT_WIDTH:
		string_to_jsval(ctx, vp, html->width);
		break;
	case JSP_HTML_IFRAME_ELEMENT_CONTENT_DOCUMENT:
		string_to_jsval(ctx, vp, html->content_document);
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLIFrameElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct IFRAME_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLIFrameElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IFRAME_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IFRAME_ELEMENT_FRAME_BORDER:
		mem_free_set(&html->frame_border, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IFRAME_ELEMENT_HEIGHT:
		mem_free_set(&html->height, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IFRAME_ELEMENT_LONG_DESC:
		mem_free_set(&html->long_desc, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IFRAME_ELEMENT_MARGIN_HEIGHT:
		mem_free_set(&html->margin_height, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IFRAME_ELEMENT_MARGIN_WIDTH:
		mem_free_set(&html->margin_width, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IFRAME_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IFRAME_ELEMENT_SCROLLING:
		mem_free_set(&html->scrolling, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IFRAME_ELEMENT_SRC:
		mem_free_set(&html->src, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IFRAME_ELEMENT_WIDTH:
		mem_free_set(&html->width, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLIFrameElement_props[] = {
	{ "align",		JSP_HTML_IFRAME_ELEMENT_ALIGN,			JSPROP_ENUMERATE },
	{ "frameBorder",	JSP_HTML_IFRAME_ELEMENT_FRAME_BORDER,		JSPROP_ENUMERATE },
	{ "height",		JSP_HTML_IFRAME_ELEMENT_HEIGHT,			JSPROP_ENUMERATE },
	{ "longDesc",		JSP_HTML_IFRAME_ELEMENT_LONG_DESC,		JSPROP_ENUMERATE },
	{ "marginHeight",	JSP_HTML_IFRAME_ELEMENT_MARGIN_HEIGHT,		JSPROP_ENUMERATE },
	{ "marginWidth",	JSP_HTML_IFRAME_ELEMENT_MARGIN_WIDTH,		JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_IFRAME_ELEMENT_NAME,			JSPROP_ENUMERATE },
	{ "scrolling",		JSP_HTML_IFRAME_ELEMENT_SCROLLING,		JSPROP_ENUMERATE },
	{ "src",		JSP_HTML_IFRAME_ELEMENT_SRC,			JSPROP_ENUMERATE },
	{ "width",		JSP_HTML_IFRAME_ELEMENT_WIDTH,			JSPROP_ENUMERATE },
	{ "contentDocument",	JSP_HTML_IFRAME_ELEMENT_CONTENT_DOCUMENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLIFrameElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLIFrameElement_class = {
	"HTMLIFrameElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLIFrameElement_getProperty, HTMLIFrameElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_IFRAME_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct IFRAME_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLIFrameElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_IFRAME_object(void *data)
{
	struct IFRAME_struct *d = data;

	mem_free_if(d->align);
	mem_free_if(d->frame_border);
	mem_free_if(d->height);
	mem_free_if(d->long_desc);
	mem_free_if(d->margin_height);
	mem_free_if(d->margin_width);
	mem_free_if(d->name);
	mem_free_if(d->scrolling);
	mem_free_if(d->src);
	mem_free_if(d->width);
	/* content_document ? */
}
