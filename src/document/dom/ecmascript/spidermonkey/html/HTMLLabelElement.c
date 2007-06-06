#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLabelElement.h"
#include "dom/node.h"

static JSBool
HTMLLabelElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct LABEL_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLLabelElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LABEL_ELEMENT_FORM:
		string_to_jsval(ctx, vp, html->form);
		/* Write me! */
		break;
	case JSP_HTML_LABEL_ELEMENT_ACCESS_KEY:
		string_to_jsval(ctx, vp, html->access_key);
		break;
	case JSP_HTML_LABEL_ELEMENT_HTML_FOR:
		string_to_jsval(ctx, vp, html->html_for);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLLabelElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct LABEL_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLLabelElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LABEL_ELEMENT_ACCESS_KEY:
		mem_free_set(&html->access_key, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_LABEL_ELEMENT_HTML_FOR:
		mem_free_set(&html->html_for, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLLabelElement_props[] = {
	{ "form",	JSP_HTML_LABEL_ELEMENT_FORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "accessKey",	JSP_HTML_LABEL_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "htmlFor",	JSP_HTML_LABEL_ELEMENT_HTML_FOR,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLLabelElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLLabelElement_class = {
	"HTMLLabelElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLLabelElement_getProperty, HTMLLabelElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_LABEL_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct LABEL_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLLabelElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_LABEL_object(void *data)
{
	struct LABEL_struct *d = data;

	/* form ? */
	mem_free_if(d->access_key);
	mem_free_if(d->html_for);
}
