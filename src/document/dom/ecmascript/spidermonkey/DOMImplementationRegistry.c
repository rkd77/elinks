#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DOMImplementationRegistry.h"

static JSBool
DOMImplementationRegistry_getDOMImplementation(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}


static JSBool
DOMImplementationRegistry_getDOMImplementationList(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSFunctionSpec DOMImplementationRegistry_funcs[] = {
	{ "getDOMImplementation",	DOMImplementationRegistry_getDOMImplementation,		1 },
	{ "getDOMImplementationList",	DOMImplementationRegistry_getDOMImplementationList,	1 },
	{ NULL }
};

const JSClass DOMIMplementationRegistry_class = {
	"DOMImplementationRegistry",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

