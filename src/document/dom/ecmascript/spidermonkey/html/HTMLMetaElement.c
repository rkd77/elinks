#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLMetaElement.h"
#include "dom/node.h"

static JSBool
HTMLMetaElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct META_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLMetaElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_META_ELEMENT_CONTENT:
		string_to_jsval(ctx, vp, html->content);
		break;
	case JSP_HTML_META_ELEMENT_HTTP_EQUIV:
		string_to_jsval(ctx, vp, html->http_equiv);
		break;
	case JSP_HTML_META_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_META_ELEMENT_SCHEME:
		string_to_jsval(ctx, vp, html->scheme);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLMetaElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct META_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLMetaElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_META_ELEMENT_CONTENT:
		mem_free_set(&html->content, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_META_ELEMENT_HTTP_EQUIV:
		mem_free_set(&html->http_equiv, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_META_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_META_ELEMENT_SCHEME:
		mem_free_set(&html->scheme, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLMetaElement_props[] = {
	{ "content",	JSP_HTML_META_ELEMENT_CONTENT,		JSPROP_ENUMERATE },
	{ "httpEquiv",	JSP_HTML_META_ELEMENT_HTTP_EQUIV,	JSPROP_ENUMERATE },
	{ "name",	JSP_HTML_META_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "scheme",	JSP_HTML_META_ELEMENT_SCHEME,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLMetaElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLMetaElement_class = {
	"HTMLMetaElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLMetaElement_getProperty, HTMLMetaElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_META_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct META_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLMetaElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_META_object(struct dom_node *node)
{
	struct META_struct *d = node->data.element.html_data;

	mem_free_if(d->content);
	mem_free_if(d->http_equiv);
	mem_free_if(d->name);
	mem_free_if(d->scheme);
}
