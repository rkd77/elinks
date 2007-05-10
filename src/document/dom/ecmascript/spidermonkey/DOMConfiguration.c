#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DOMConfiguration.h"

static JSBool
DOMConfiguration_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_PARAMETER_NAMES:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

static JSBool
DOMConfiguration_setParameter(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
DOMConfiguration_getParameter(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
DOMConfiguration_canSetParameter(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec DOMConfiguration_props[] = {
	{ "parameterNames",	JSP_DOM_PARAMETER_NAMES,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec DOMConfiguration_funcs[] = {
	{ "setParameter",	DOMConfiguration_setParameter,		2 },
	{ "getParameter",	DOMConfiguration_getParameter,		1 },
	{ "canSetParameter",	DOMConfiguration_canSetParameter,	2 },
	{ NULL }
};

const JSClass DOMConfiguration_class = {
	"DOMConfiguration",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	DOMConfiguration_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

