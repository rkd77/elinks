#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLButtonElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"

static JSBool
HTMLButtonElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BUTTON_ELEMENT_FORM:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_VALUE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLButtonElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BUTTON_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_BUTTON_ELEMENT_VALUE:
		/* Write me! */
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
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

