#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLinkElement.h"
#include "dom/node.h"

static JSBool
HTMLLinkElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct LINK_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLLinkElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LINK_ELEMENT_DISABLED:
		boolean_to_jsval(ctx, vp, html->disabled);
		break;
	case JSP_HTML_LINK_ELEMENT_CHARSET:
		string_to_jsval(ctx, vp, html->charset);
		break;
	case JSP_HTML_LINK_ELEMENT_HREF:
		string_to_jsval(ctx, vp, html->href);
		break;
	case JSP_HTML_LINK_ELEMENT_HREFLANG:
		string_to_jsval(ctx, vp, html->hreflang);
		break;
	case JSP_HTML_LINK_ELEMENT_MEDIA:
		string_to_jsval(ctx, vp, html->media);
		break;
	case JSP_HTML_LINK_ELEMENT_REL:
		string_to_jsval(ctx, vp, html->rel);
		break;
	case JSP_HTML_LINK_ELEMENT_REV:
		string_to_jsval(ctx, vp, html->rev);
		break;
	case JSP_HTML_LINK_ELEMENT_TARGET:
		string_to_jsval(ctx, vp, html->target);
		break;
	case JSP_HTML_LINK_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLLinkElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct LINK_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLLinkElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LINK_ELEMENT_DISABLED:
		html->disabled = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_LINK_ELEMENT_CHARSET:
		mem_free_set(&html->charset, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_LINK_ELEMENT_HREF:
		mem_free_set(&html->href, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_LINK_ELEMENT_HREFLANG:
		mem_free_set(&html->hreflang, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_LINK_ELEMENT_MEDIA:
		mem_free_set(&html->media, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_LINK_ELEMENT_REL:
		mem_free_set(&html->rel, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_LINK_ELEMENT_REV:
		mem_free_set(&html->rev, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_LINK_ELEMENT_TARGET:
		mem_free_set(&html->target, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_LINK_ELEMENT_TYPE:
		mem_free_set(&html->type, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLLinkElement_props[] = {
	{ "disabled",	JSP_HTML_LINK_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "charset",	JSP_HTML_LINK_ELEMENT_CHARSET,	JSPROP_ENUMERATE },
	{ "href",	JSP_HTML_LINK_ELEMENT_HREF,	JSPROP_ENUMERATE },
	{ "hreflang",	JSP_HTML_LINK_ELEMENT_HREFLANG,	JSPROP_ENUMERATE },
	{ "media",	JSP_HTML_LINK_ELEMENT_MEDIA,	JSPROP_ENUMERATE },
	{ "rel",	JSP_HTML_LINK_ELEMENT_REL,	JSPROP_ENUMERATE },
	{ "rev",	JSP_HTML_LINK_ELEMENT_REV,	JSPROP_ENUMERATE },
	{ "target",	JSP_HTML_LINK_ELEMENT_TARGET,	JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_LINK_ELEMENT_TYPE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLLinkElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLLinkElement_class = {
	"HTMLLinkElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLLinkElement_getProperty, HTMLLinkElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_LINK_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct LINK_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLLinkElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_LINK_object(void *data)
{
	struct LINK_struct *d = data;

	mem_free_if(d->charset);
	mem_free_if(d->href);
	mem_free_if(d->hreflang);
	mem_free_if(d->media);
	mem_free_if(d->rel);
	mem_free_if(d->rev);
	mem_free_if(d->target);
	mem_free_if(d->type);
}
