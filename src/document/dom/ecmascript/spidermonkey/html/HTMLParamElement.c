#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLParamElement.h"

static JSBool
HTMLParamElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_PARAM_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_PARAM_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_PARAM_ELEMENT_VALUE:
		/* Write me! */
		break;
	case JSP_HTML_PARAM_ELEMENT_VALUE_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLParamElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_PARAM_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_PARAM_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_PARAM_ELEMENT_VALUE:
		/* Write me! */
		break;
	case JSP_HTML_PARAM_ELEMENT_VALUE_TYPE:
		/* Write me! */
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

