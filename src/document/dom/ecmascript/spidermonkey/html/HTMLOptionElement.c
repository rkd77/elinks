#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOptionElement.h"
#include "dom/node.h"

static JSBool
HTMLOptionElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct OPTION_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLOptionElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OPTION_ELEMENT_FORM:
		if (html->form)
			object_to_jsval(ctx, vp, html->form->ecmascript_obj);
		else
			undef_to_jsval(ctx, vp);
		break;
	case JSP_HTML_OPTION_ELEMENT_DEFAULT_SELECTED:
		boolean_to_jsval(ctx, vp, html->default_selected);
		break;
	case JSP_HTML_OPTION_ELEMENT_TEXT:
		string_to_jsval(ctx, vp, html->text);
		break;
	case JSP_HTML_OPTION_ELEMENT_INDEX:
		int_to_jsval(ctx, vp, html->index);
		break;
	case JSP_HTML_OPTION_ELEMENT_DISABLED:
		boolean_to_jsval(ctx, vp, html->disabled);
		break;
	case JSP_HTML_OPTION_ELEMENT_LABEL:
		string_to_jsval(ctx, vp, html->label);
		break;
	case JSP_HTML_OPTION_ELEMENT_SELECTED:
		boolean_to_jsval(ctx, vp, html->selected);
		break;
	case JSP_HTML_OPTION_ELEMENT_VALUE:
		string_to_jsval(ctx, vp, html->value);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLOptionElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct OPTION_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLOptionElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OPTION_ELEMENT_DEFAULT_SELECTED:
		html->default_selected = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_OPTION_ELEMENT_DISABLED:
		html->disabled = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_OPTION_ELEMENT_LABEL:
		mem_free_set(&html->label, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_OPTION_ELEMENT_SELECTED:
		html->selected = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_OPTION_ELEMENT_VALUE:
		mem_free_set(&html->value, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLOptionElement_props[] = {
	{ "form",		JSP_HTML_OPTION_ELEMENT_FORM,			JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "defaultSelected",	JSP_HTML_OPTION_ELEMENT_DEFAULT_SELECTED,	JSPROP_ENUMERATE },
	{ "text",		JSP_HTML_OPTION_ELEMENT_TEXT,			JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "index",		JSP_HTML_OPTION_ELEMENT_INDEX,			JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "disabled",		JSP_HTML_OPTION_ELEMENT_DISABLED,		JSPROP_ENUMERATE },
	{ "label",		JSP_HTML_OPTION_ELEMENT_LABEL,			JSPROP_ENUMERATE },
	{ "selected",		JSP_HTML_OPTION_ELEMENT_SELECTED,		JSPROP_ENUMERATE },
	{ "value",		JSP_HTML_OPTION_ELEMENT_VALUE,			JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLOptionElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLOptionElement_class = {
	"HTMLOptionElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLOptionElement_getProperty, HTMLOptionElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_OPTION_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct OPTION_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLOptionElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_OPTION_object(void *data)
{
	struct OPTION_struct *d = data;

	/* form ? */
	mem_free_if(d->text);
	mem_free_if(d->label);
	mem_free_if(d->value);
}
