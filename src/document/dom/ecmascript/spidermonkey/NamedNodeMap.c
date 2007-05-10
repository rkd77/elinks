#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/NamedNodeMap.h"

static JSBool
NamedNodeMap_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
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
NamedNodeMap_getNamedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
NamedNodeMap_setNamedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
NamedNodeMap_removeNamedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
NamedNodeMap_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
NamedNodeMap_getNamedItemNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
NamedNodeMap_setNamedItemNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
NamedNodeMap_removeNamedItemNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec NamedNodeMap_props[] = {
	{ "length",	JSP_DOM_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec NamedNodeMap_funcs[] = {
	{ "getNamedItem",	NamedNodeMap_getNamedItem,	1 },
	{ "setNamedItem",	NamedNodeMap_setNamedItem,	1 },
	{ "removeNamedItem",	NamedNodeMap_removeNamedItem,	1 },
	{ "item",		NamedNodeMap_item,		1 },
	{ "getNamedItemNS",	NamedNodeMap_getNamedItemNS,	2 },
	{ "setNamedItemNS",	NamedNodeMap_setNamedItemNS,	1 },
	{ "removeNamedItemNS",	NamedNodeMap_removeNamedItemNS,	2 },
	{ NULL }
};

const JSClass NamedNodeMap_class = {
	"NamedNodeMap",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	NamedNodeMap_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

