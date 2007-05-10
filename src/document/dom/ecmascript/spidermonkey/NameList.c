#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/NameList.h"

static JSBool
NameList_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_LENGTH:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

static JSBool
NameList_getName(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
NameList_getNamespaceURI(JSContext *ctx, JSObject *obj, uintN argc,
			jsval *argv, jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
NameList_contains(JSContext *ctx, JSObject *obj, uintN argc,
			jsval *argv, jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
NameList_containsNS(JSContext *ctx, JSObject *obj, uintN argc,
			jsval *argv, jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec NameList_props[] = {
	{ "length",	JSP_DOM_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec NameList_funcs[] = {
	{ "getName",		NameList_getName,		1 },
	{ "getNamespaceURI",	NameList_getNamespaceURI,	1 },
	{ "contains",		NameList_contains,		1 },
	{ "containsNS",		NameList_containsNS,		2 },
	{ NULL }
};

const JSClass NameList_class = {
	"NameList",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	NameList_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

