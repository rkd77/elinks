#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DOMImplementationSource.h"

static JSBool
DOMImplementationSource_getDOMImplementation(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
DOMImplementationSource_getDOMImplementationList(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSFunctionSpec DOMImplementationSource_funcs[] = {
	{ "getDOMImplementation",	DOMImplementationSource_getDOMImplementation,		1 },
	{ "getDOMImplementationList",	DOMImplementationSource_getDOMImplementationList,	1 },
	{ NULL }
};

const JSClass DOMImplementationSource_class = {
	"DOMImplementationSource",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

