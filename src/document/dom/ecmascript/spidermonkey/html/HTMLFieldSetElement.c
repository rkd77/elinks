#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFieldSetElement.h"
#include "dom/node.h"

static JSBool
HTMLFieldSetElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct FIELDSET_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLFieldSetElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FIELD_SET_ELEMENT_FORM:
		if (html->form)
			object_to_jsval(ctx, vp, html->form->ecmascript_obj);
		else
			undef_to_jsval(ctx, vp);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLFieldSetElement_props[] = {
	{ "form",	JSP_HTML_FIELD_SET_ELEMENT_FORM,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLFieldSetElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLFieldSetElement_class = {
	"HTMLFieldSetElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLFieldSetElement_getProperty, HTMLElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_FIELDSET_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct FIELDSET_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLFieldSetElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_FIELDSET_object(void *data)
{
}
