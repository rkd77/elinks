#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFormElement.h"

static JSBool
HTMLFormElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FORM_ELEMENT_ELEMENTS:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_LENGTH:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_ACCEPT_CHARSET:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_ACTION:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_ENCTYPE:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_METHOD:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_TARGET:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLFormElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FORM_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_ACCEPT_CHARSET:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_ACTION:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_ENCTYPE:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_METHOD:
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_TARGET:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLFormElement_submit(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLFormElement_reset(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLFormElement_props[] = {
	{ "elements",		JSP_HTML_FORM_ELEMENT_ELEMENTS,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "length",		JSP_HTML_FORM_ELEMENT_LENGTH,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "name",		JSP_HTML_FORM_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "acceptCharset",	JSP_HTML_FORM_ELEMENT_ACCEPT_CHARSET,	JSPROP_ENUMERATE },
	{ "action",		JSP_HTML_FORM_ELEMENT_ACTION,		JSPROP_ENUMERATE },
	{ "enctype",		JSP_HTML_FORM_ELEMENT_ENCTYPE,		JSPROP_ENUMERATE },
	{ "method",		JSP_HTML_FORM_ELEMENT_METHOD,	JSPROP_ENUMERATE },
	{ "target",		JSP_HTML_FORM_ELEMENT_TARGET,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLFormElement_funcs[] = {
	{ "submit",	HTMLFormElement_submit,	0 },
	{ "reset",	HTMLFormElement_reset,	0 },
	{ NULL }
};

const JSClass HTMLFormElement_class = {
	"HTMLFormElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLFormElement_getProperty, HTMLFormElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

