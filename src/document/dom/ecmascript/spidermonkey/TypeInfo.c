/* SpiderMonkey's DOM TypeInfo implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/TypeInfo.h"

static JSBool
TypeInfo_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_TYPE_NAME:
		/* Write me! */
		break;
	case JSP_DOM_TYPE_NAMESPACE:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

static JSBool
TypeInfo_isDerivedFrom(JSContext *ctx, JSObject *obj, uintN argc,
			jsval *argv, jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec TypeInfo_props[] = {
	{ "typeName",		JSP_DOM_TYPE_NAME,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "typeNamespace",	JSP_DOM_TYPE_NAMESPACE,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec TypeInfo_funcs[] = {
	{ "isDerivedFrom",	TypeInfo_isDerivedFrom,	3},
	{ NULL }
};

const JSClass TypeInfo_class = {
	"TypeInfo",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	TypeInfo_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

