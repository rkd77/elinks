#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOptionsCollection.h"

static JSBool
HTMLOptionsCollection_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OPTIONS_COLLECTION_LENGTH:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

static JSBool
HTMLOptionsCollection_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLOptionsCollection_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLOptionsCollection_props[] = {
	{ "length",	JSP_HTML_OPTIONS_COLLECTION_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLOptionsCollection_funcs[] = {
	{ "item",	HTMLOptionsCollection_item,		1 },
	{ "namedItem",	HTMLOptionsCollection_namedItem,	1 },
	{ NULL }
};

const JSClass HTMLOptionsCollection_class = {
	"HTMLOptionsCollection",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLOptionsCollection_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

