#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOptionElement.h"

static JSBool
HTMLOptionElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OPTION_ELEMENT_FORM:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_DEFAULT_SELECTED:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_TEXT:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_LABEL:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_SELECTED:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_VALUE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLOptionElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OPTION_ELEMENT_DEFAULT_SELECTED:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_LABEL:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_SELECTED:
		/* Write me! */
		break;
	case JSP_HTML_OPTION_ELEMENT_VALUE:
		/* Write me! */
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

