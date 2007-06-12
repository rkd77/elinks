#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLUListElement.h"
#include "dom/node.h"

static JSBool
HTMLUListElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct UL_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLUListElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ULIST_ELEMENT_COMPACT:
		boolean_to_jsval(ctx, vp, html->compact);
		break;
	case JSP_HTML_ULIST_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLUListElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct UL_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLUListElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ULIST_ELEMENT_COMPACT:
		html->compact = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_ULIST_ELEMENT_TYPE:
		mem_free_set(&html->type, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLUListElement_props[] = {
	{ "compact",		JSP_HTML_ULIST_ELEMENT_COMPACT,	JSPROP_ENUMERATE },
	{ "type",		JSP_HTML_ULIST_ELEMENT_TYPE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSClass HTMLUListElement_class = {
	"HTMLUListElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLUListElement_getProperty, HTMLUListElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_UL_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct UL_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLUListElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_UL_object(struct dom_node *node)
{
	struct UL_struct *d = node->data.element.html_data;

	mem_free_if(d->type);
}
