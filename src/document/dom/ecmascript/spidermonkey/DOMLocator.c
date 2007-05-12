#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DOMLocator.h"

static JSBool
DOMLocator_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_LOCATOR_LINE_NUMBER:
		/* Write me! */
		break;
	case JSP_DOM_LOCATOR_COLUMN_NUMBER:
		/* Write me! */
		break;
	case JSP_DOM_LOCATOR_BYTE_OFFSET:
		/* Write me! */
		break;
	case JSP_DOM_LOCATOR_UTF16_OFFSET:
		/* Write me! */
		break;
	case JSP_DOM_LOCATOR_RELATED_NODE:
		/* Write me! */
		break;
	case JSP_DOM_LOCATOR_URI:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

const JSPropertySpec DOMLocator_props[] = {
	{ "lineNumber",		JSP_DOM_LOCATOR_LINE_NUMBER,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "columnNumber",	JSP_DOM_LOCATOR_COLUMN_NUMBER,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "byteOffset",		JSP_DOM_LOCATOR_BYTE_OFFSET,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "utf16Offset",	JSP_DOM_LOCATOR_UTF16_OFFSET,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "relatedNode",	JSP_DOM_LOCATOR_RELATED_NODE,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "uri",		JSP_DOM_LOCATOR_URI,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec DOMLocator_funcs[] = {
	{ NULL }
};

const JSClass DOMLocator_class = {
	"DOMLocator",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	DOMLocator_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

