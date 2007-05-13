#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLabelElement.h"

static JSBool
HTMLLabelElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LABEL_ELEMENT_FORM:
		/* Write me! */
		break;
	case JSP_HTML_LABEL_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_LABEL_ELEMENT_HTML_FOR:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLLabelElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LABEL_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_LABEL_ELEMENT_HTML_FOR:
		/* Write me! */
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

