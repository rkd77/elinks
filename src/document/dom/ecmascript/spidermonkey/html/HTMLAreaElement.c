#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLAreaElement.h"
#include "dom/node.h"

static JSBool
HTMLAreaElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct AREA_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLAreaElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_AREA_ELEMENT_ACCESS_KEY:
		string_to_jsval(ctx, vp, html->access_key);
		break;
	case JSP_HTML_AREA_ELEMENT_ALT:
		string_to_jsval(ctx, vp, html->alt);
		break;
	case JSP_HTML_AREA_ELEMENT_COORDS:
		string_to_jsval(ctx, vp, html->coords);
		break;
	case JSP_HTML_AREA_ELEMENT_HREF:
		string_to_jsval(ctx, vp, html->href);
		break;
	case JSP_HTML_AREA_ELEMENT_NO_HREF:
		boolean_to_jsval(ctx, vp, html->no_href);
		break;
	case JSP_HTML_AREA_ELEMENT_SHAPE:
		string_to_jsval(ctx, vp, html->shape);
		break;
	case JSP_HTML_AREA_ELEMENT_TAB_INDEX:
		int_to_jsval(ctx, vp, html->tab_index);
		break;
	case JSP_HTML_AREA_ELEMENT_TARGET:
		string_to_jsval(ctx, vp, html->target);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLAreaElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct AREA_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLAreaElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_AREA_ELEMENT_ACCESS_KEY:
		mem_free_set(&html->access_key, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_AREA_ELEMENT_ALT:
		mem_free_set(&html->alt, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_AREA_ELEMENT_COORDS:
		mem_free_set(&html->coords, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_AREA_ELEMENT_HREF:
		mem_free_set(&html->href, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_AREA_ELEMENT_NO_HREF:
		html->no_href = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_AREA_ELEMENT_SHAPE:
		mem_free_set(&html->shape, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_AREA_ELEMENT_TAB_INDEX:
		return JS_ValueToInt32(ctx, *vp, &html->tab_index);
	case JSP_HTML_AREA_ELEMENT_TARGET:
		mem_free_set(&html->target, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLAreaElement_props[] = {
	{ "accessKey",	JSP_HTML_AREA_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "alt",	JSP_HTML_AREA_ELEMENT_ALT,		JSPROP_ENUMERATE },
	{ "coords",	JSP_HTML_AREA_ELEMENT_COORDS,		JSPROP_ENUMERATE },
	{ "href",	JSP_HTML_AREA_ELEMENT_HREF,		JSPROP_ENUMERATE },
	{ "noHref",	JSP_HTML_AREA_ELEMENT_NO_HREF,		JSPROP_ENUMERATE },
	{ "shape",	JSP_HTML_AREA_ELEMENT_SHAPE,		JSPROP_ENUMERATE },
	{ "tabIndex",	JSP_HTML_AREA_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ "target",	JSP_HTML_AREA_ELEMENT_TARGET,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLAreaElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLAreaElement_class = {
	"HTMLAreaElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLAreaElement_getProperty, HTMLAreaElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_AREA_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct AREA_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLAreaElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_AREA_object(void *data)
{
	struct AREA_struct *d = data;

	mem_free_if(d->access_key);
	mem_free_if(d->alt);
	mem_free_if(d->coords);
	mem_free_if(d->href);
	mem_free_if(d->shape);
	mem_free_if(d->target);
}
