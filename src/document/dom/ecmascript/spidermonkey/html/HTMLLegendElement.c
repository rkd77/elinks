#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLegendElement.h"

static JSBool
HTMLLegendElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LEGEND_ELEMENT_FORM:
		/* Write me! */
		break;
	case JSP_HTML_LEGEND_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_LEGEND_ELEMENT_ALIGN:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLLegendElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LEGEND_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_LEGEND_ELEMENT_ALIGN:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLLegendElement_props[] = {
	{ "form",	JSP_HTML_LEGEND_ELEMENT_FORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "accessKey",	JSP_HTML_LEGEND_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "align",	JSP_HTML_LEGEND_ELEMENT_ALIGN,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLLegendElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLLegendElement_class = {
	"HTMLLegendElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLLegendElement_getProperty, HTMLLegendElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

