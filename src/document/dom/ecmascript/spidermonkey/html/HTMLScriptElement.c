#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLScriptElement.h"

static JSBool
HTMLScriptElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_SCRIPT_ELEMENT_TEXT:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_HTML_FOR:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_EVENT:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_CHARSET:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_DEFER:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_SRC:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLScriptElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_SCRIPT_ELEMENT_TEXT:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_HTML_FOR:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_EVENT:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_CHARSET:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_DEFER:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_SRC:
		/* Write me! */
		break;
	case JSP_HTML_SCRIPT_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLScriptElement_props[] = {
	{ "text",	JSP_HTML_SCRIPT_ELEMENT_TEXT,		JSPROP_ENUMERATE },
	{ "htmlFor",	JSP_HTML_SCRIPT_ELEMENT_HTML_FOR,	JSPROP_ENUMERATE },
	{ "event",	JSP_HTML_SCRIPT_ELEMENT_EVENT	,	JSPROP_ENUMERATE },
	{ "charset",	JSP_HTML_SCRIPT_ELEMENT_CHARSET,	JSPROP_ENUMERATE },
	{ "defer",	JSP_HTML_SCRIPT_ELEMENT_DEFER,		JSPROP_ENUMERATE },
	{ "src",	JSP_HTML_SCRIPT_ELEMENT_SRC,		JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_SCRIPT_ELEMENT_TYPE,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLScriptElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLScriptElement_class = {
	"HTMLScriptElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLScriptElement_getProperty, HTMLScriptElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

