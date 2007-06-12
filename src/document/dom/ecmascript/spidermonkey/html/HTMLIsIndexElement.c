#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFormElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLIsIndexElement.h"
#include "dom/node.h"

static JSBool
HTMLIsIndexElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct ISINDEX_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLIsIndexElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IS_INDEX_ELEMENT_FORM:
		if (html->form)
			object_to_jsval(ctx, vp, html->form->ecmascript_obj);
		else
			undef_to_jsval(ctx, vp);
		break;
	case JSP_HTML_IS_INDEX_ELEMENT_PROMPT:
		string_to_jsval(ctx, vp, html->prompt);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLIsIndexElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct ISINDEX_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLIsIndexElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IS_INDEX_ELEMENT_PROMPT:
		mem_free_set(&html->prompt, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLIsIndexElement_props[] = {
	{ "form",	JSP_HTML_IS_INDEX_ELEMENT_FORM,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "prompt",	JSP_HTML_IS_INDEX_ELEMENT_PROMPT,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLIsIndexElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLIsIndexElement_class = {
	"HTMLIsIndexElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLIsIndexElement_getProperty, HTMLIsIndexElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_ISINDEX_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct ISINDEX_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLIsIndexElement_class, o->HTMLElement_object, NULL);
		register_form_element(node);
	}
}

void
done_ISINDEX_object(struct dom_node *node)
{
	struct ISINDEX_struct *d = node->data.element.html_data;

	unregister_form_element(d->form, node);
	mem_free_if(d->prompt);
}
