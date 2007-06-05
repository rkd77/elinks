#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBaseElement.h"
#include "dom/node.h"

static JSBool
HTMLBaseElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct BASE_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLBaseElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BASE_ELEMENT_HREF:
		string_to_jsval(ctx, vp, html->href);
		break;
	case JSP_HTML_BASE_ELEMENT_TARGET:
		string_to_jsval(ctx, vp, html->target);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLBaseElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct BASE_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLBaseElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BASE_ELEMENT_HREF:
		mem_free_set(&html->href, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BASE_ELEMENT_TARGET:
		mem_free_set(&html->target, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLBaseElement_props[] = {
	{ "href",	JSP_HTML_BASE_ELEMENT_HREF,	JSPROP_ENUMERATE },
	{ "target",	JSP_HTML_BASE_ELEMENT_TARGET,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLBaseElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLBaseElement_class = {
	"HTMLBaseElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLBaseElement_getProperty, HTMLBaseElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_BASE_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct BASE_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLBaseElement_class, o->HTMLElement_object, NULL);
	}
}

