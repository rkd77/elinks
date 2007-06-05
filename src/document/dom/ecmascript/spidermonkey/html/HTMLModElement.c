#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLModElement.h"
#include "dom/node.h"

static JSBool
HTMLModElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct MOD_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLModElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_MOD_ELEMENT_CITE:
		string_to_jsval(ctx, vp, html->cite);
		break;
	case JSP_HTML_MOD_ELEMENT_DATE_TIME:
		string_to_jsval(ctx, vp, html->date_time);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLModElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct MOD_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLModElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_MOD_ELEMENT_CITE:
		mem_free_set(&html->cite, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_MOD_ELEMENT_DATE_TIME:
		mem_free_set(&html->date_time, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLModElement_props[] = {
	{ "cite",	JSP_HTML_MOD_ELEMENT_CITE,	JSPROP_ENUMERATE },
	{ "dateTime",	JSP_HTML_MOD_ELEMENT_DATE_TIME,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLModElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLModElement_class = {
	"HTMLModElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLModElement_getProperty, HTMLModElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_MOD_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct MOD_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLModElement_class, o->HTMLElement_object, NULL);
	}
}

