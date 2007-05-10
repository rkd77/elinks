#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DOMImplementationList.h"

static JSBool
DOMImplementationList_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
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
DOMImplementationList_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec DOMImplementationList_props[] = {
	{ "length",	JSP_DOM_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec DOMIMplementationList_funcs[] = {
	{ "item",	DOMImplementationList_item,	1 },
	{ NULL }
};

const JSClass DOMImplementationList_class = {
	"DOMImplementationList",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	DOMImplementationList_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

