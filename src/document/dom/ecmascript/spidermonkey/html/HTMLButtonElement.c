#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLButtonElement.h"
#include "dom/node.h"

static JSBool
HTMLButtonElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct BUTTON_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLButtonElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BUTTON_ELEMENT_FORM:
		if (html->form)
			object_to_jsval(ctx, vp, html->form->ecmascript_obj);
		else
			undef_to_jsval(ctx, vp);
		break;
	case JSP_HTML_BUTTON_ELEMENT_ACCESS_KEY:
		string_to_jsval(ctx, vp, html->access_key);
		break;
	case JSP_HTML_BUTTON_ELEMENT_DISABLED:
		boolean_to_jsval(ctx, vp, html->disabled);
		break;
	case JSP_HTML_BUTTON_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_BUTTON_ELEMENT_TAB_INDEX:
		int_to_jsval(ctx, vp, html->tab_index);
		break;
	case JSP_HTML_BUTTON_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	case JSP_HTML_BUTTON_ELEMENT_VALUE:
		string_to_jsval(ctx, vp, html->value);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLButtonElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct BUTTON_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLButtonElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BUTTON_ELEMENT_ACCESS_KEY:
		mem_free_set(&html->access_key, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BUTTON_ELEMENT_DISABLED:
		html->disabled = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_BUTTON_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_BUTTON_ELEMENT_TAB_INDEX:
		return JS_ValueToInt32(ctx, *vp, &html->tab_index);
	case JSP_HTML_BUTTON_ELEMENT_VALUE:
		mem_free_set(&html->value, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLButtonElement_props[] = {
	{ "form",	JSP_HTML_BUTTON_ELEMENT_FORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "accessKey",	JSP_HTML_BUTTON_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "disabled",	JSP_HTML_BUTTON_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "name",	JSP_HTML_BUTTON_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "tabIndex",	JSP_HTML_BUTTON_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_BUTTON_ELEMENT_TYPE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "value",	JSP_HTML_BUTTON_ELEMENT_VALUE,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLButtonElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLButtonElement_class = {
	"HTMLButtonElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLButtonElement_getProperty, HTMLButtonElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_BUTTON_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct BUTTON_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLButtonElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_BUTTON_object(struct dom_node *node)
{
	struct BUTTON_struct *d = node->data.element.html_data;

	/* form musn't be freed */
	mem_free_if(d->access_key);
	mem_free_if(d->name);
	mem_free_if(d->type);
	mem_free_if(d->value);
	
}
