#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/NodeList.h"

static JSBool
NodeList_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_NODE_LIST_LENGTH:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

static JSBool
NodeList_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec NodeList_props[] = {
	{ "length",	JSP_NODE_LIST_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec NodeList_funcs[] = {
	{ "item",	NodeList_item,	1 },
	{ NULL }
};

const JSClass NodeList_class = {
	"NodeList",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	NodeList_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

