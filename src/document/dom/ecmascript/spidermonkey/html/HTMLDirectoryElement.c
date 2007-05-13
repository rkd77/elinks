#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLDirectoryElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"

static JSBool
HTMLDirectoryElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_DIRECTORY_ELEMENT_COMPACT:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLDirectoryElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_DIRECTORY_ELEMENT_COMPACT:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLDirectoryElement_props[] = {
	{ "compact",	JSP_HTML_DIRECTORY_ELEMENT_COMPACT,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLDirectoryElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLDirectoryElement_class = {
	"HTMLDirectoryElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLDirectoryElement_getProperty, HTMLDirectoryElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

