#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DOMException.h"

static JSBool
DOMException_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_CODE:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

static JSBool
DOMException_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_CODE:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

const JSPropertySpec DOMException_props[] = {
	{ "code",	JSP_DOM_CODE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSClass DOMException_class = {
	"DOMException",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	DOMException_getProperty, DOMException_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

