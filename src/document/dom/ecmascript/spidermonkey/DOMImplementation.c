#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DOMImplementation.h"

static JSBool
DOMImplementation_hasFeature(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
DOMImplementation_createDocumentType(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
DOMImplementation_createDocument(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
DOMImplementation_getFeature(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec DOMImplementation_props[] = {
	{ NULL }
};

const JSFunctionSpec DOMImplementation_funcs[] = {
	{ "hasFeature",		DOMImplementation_hasFeature,		2 },
	{ "createDocumentType",	DOMImplementation_createDocumentType,	3 },
	{ "createDocument",	DOMImplementation_createDocument,	3 },
	{ "getFeature",		DOMImplementation_getFeature,		2 },
	{ NULL }
};

const JSClass DOMImplementation_class = {
	"DOMImplementation",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

