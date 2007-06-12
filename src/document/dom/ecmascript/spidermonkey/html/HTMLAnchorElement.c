#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLAnchorElement.h"
#include "dom/node.h"

static JSBool
HTMLAnchorElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct A_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLAnchorElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ANCHOR_ELEMENT_ACCESS_KEY:
		string_to_jsval(ctx, vp, html->access_key);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_CHARSET:
		string_to_jsval(ctx, vp, html->charset);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_COORDS:
		string_to_jsval(ctx, vp, html->coords);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_HREF:
		string_to_jsval(ctx, vp, html->href);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_HREFLANG:
		string_to_jsval(ctx, vp, html->hreflang);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_REL:
		string_to_jsval(ctx, vp, html->rel);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_REV:
		string_to_jsval(ctx, vp, html->rev);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_SHAPE:
		string_to_jsval(ctx, vp, html->shape);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TAB_INDEX:
		int_to_jsval(ctx, vp, html->tab_index);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TARGET:
		string_to_jsval(ctx, vp, html->target);
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLAnchorElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct A_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLAnchorElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;


	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ANCHOR_ELEMENT_ACCESS_KEY:
		mem_free_set(&html->access_key, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_CHARSET:
		mem_free_set(&html->charset, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_COORDS:
		mem_free_set(&html->coords, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_HREF:
		mem_free_set(&html->href, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_HREFLANG:
		mem_free_set(&html->hreflang, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_REL:
		mem_free_set(&html->rel, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_REV:
		mem_free_set(&html->rev, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_SHAPE:
		mem_free_set(&html->shape, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TAB_INDEX:
		return JS_ValueToInt32(ctx, *vp, &html->tab_index);
	case JSP_HTML_ANCHOR_ELEMENT_TARGET:
		mem_free_set(&html->target, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TYPE:
		mem_free_set(&html->type, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLAnchorElement_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLAnchorElement_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLAnchorElement_props[] = {
	{ "accessKey",	JSP_HTML_ANCHOR_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "charset",	JSP_HTML_ANCHOR_ELEMENT_CHARSET,	JSPROP_ENUMERATE },
	{ "coords",	JSP_HTML_ANCHOR_ELEMENT_COORDS,		JSPROP_ENUMERATE },
	{ "href",	JSP_HTML_ANCHOR_ELEMENT_HREF,		JSPROP_ENUMERATE },
	{ "hreflang",	JSP_HTML_ANCHOR_ELEMENT_HREFLANG,	JSPROP_ENUMERATE },
	{ "name",	JSP_HTML_ANCHOR_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "rel",	JSP_HTML_ANCHOR_ELEMENT_REL,		JSPROP_ENUMERATE },
	{ "rev",	JSP_HTML_ANCHOR_ELEMENT_REV,		JSPROP_ENUMERATE },
	{ "shape",	JSP_HTML_ANCHOR_ELEMENT_SHAPE,		JSPROP_ENUMERATE },
	{ "tabIndex",	JSP_HTML_ANCHOR_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ "target",	JSP_HTML_ANCHOR_ELEMENT_TARGET,		JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_ANCHOR_ELEMENT_TYPE,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLAnchorElement_funcs[] = {
	{ "blur",	HTMLAnchorElement_blur,	0 },
	{ "focus",	HTMLAnchorElement_focus,	0 },
	{ NULL }
};

const JSClass HTMLAnchorElement_class = {
	"HTMLAnchorElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLAnchorElement_getProperty, HTMLAnchorElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_A_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct A_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLAnchorElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_A_object(struct dom_node *node)
{
	struct A_struct *d = node->data.element.html_data;

	mem_free_if(d->access_key);
	mem_free_if(d->charset);
	mem_free_if(d->coords);
	mem_free_if(d->href);
	mem_free_if(d->hreflang);
	mem_free_if(d->name);
	mem_free_if(d->rel);
	mem_free_if(d->rev);
	mem_free_if(d->shape);
	mem_free_if(d->target);
	mem_free_if(d->type);
}
