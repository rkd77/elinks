#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DOMError.h"

static JSBool
DOMError_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_SEVERITY:
		/* Write me! */
		break;
	case JSP_DOM_MESSAGE:
		/* Write me! */
		break;
	case JSP_DOM_TYPE:
		/* Write me! */
		break;
	case JSP_DOM_RELATED_EXCEPTION:
		/* Write me! */
		break;
	case JSP_DOM_RELATED_DATA:
		/* Write me! */
		break;
	case JSP_DOM_LOCATION:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

const JSPropertySpec DOMError_props[] = {
	{ "severity",		JSP_DOM_SEVERITY,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "message",		JSP_DOM_MESSAGE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "type",		JSP_DOM_TYPE,			JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "relatedException",	JSP_DOM_RELATED_EXCEPTION,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "relatedData",	JSP_DOM_RELATED_DATA,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "location",		JSP_DOM_LOCATION,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSClass DOMError_class = {
	"DOMError",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	DOMError_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

