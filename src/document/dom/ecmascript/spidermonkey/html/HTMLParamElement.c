#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLParamElement.h"
#include "dom/node.h"

static JSBool
HTMLParamElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct PARAM_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLParamElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_PARAM_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_PARAM_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	case JSP_HTML_PARAM_ELEMENT_VALUE:
		string_to_jsval(ctx, vp, html->value);
		break;
	case JSP_HTML_PARAM_ELEMENT_VALUE_TYPE:
		string_to_jsval(ctx, vp, html->value_type);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLParamElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct PARAM_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLParamElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_PARAM_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_PARAM_ELEMENT_TYPE:
		mem_free_set(&html->type, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_PARAM_ELEMENT_VALUE:
		mem_free_set(&html->value, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_PARAM_ELEMENT_VALUE_TYPE:
		mem_free_set(&html->value_type, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLParamElement_props[] = {
	{ "name",	JSP_HTML_PARAM_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_PARAM_ELEMENT_TYPE,		JSPROP_ENUMERATE },
	{ "value",	JSP_HTML_PARAM_ELEMENT_VALUE,		JSPROP_ENUMERATE },
	{ "valueType",	JSP_HTML_PARAM_ELEMENT_VALUE_TYPE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLParamElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLParamElement_class = {
	"HTMLParamElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLParamElement_getProperty, HTMLParamElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};
