#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/Notation.h"

static JSBool
Notation_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_NOTATION_PUBLIC_ID:
		/* Write me! */
		break;
	case JSP_NOTATION_SYSTEM_ID:
		/* Write me! */
		break;
	default:
		return Node_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec Notation_props[] = {
	{ "publicId",	JSP_NOTATION_PUBLIC_ID,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "systemId",	JSP_NOTATION_SYSTEM_ID,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec Notation_funcs[] = {
	{ NULL }
};

const JSClass Notation_class = {
	"Notation",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	Notation_getProperty, Node_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

