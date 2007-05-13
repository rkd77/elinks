#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Entity.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"

static JSBool
Entity_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_ENTITY_PUBLIC_ID:
		/* Write me! */
		break;
	case JSP_ENTITY_SYSTEM_ID:
		/* Write me! */
		break;
	case JSP_ENTITY_NOTATION_NAME:
		/* Write me! */
		break;
	case JSP_ENTITY_INPUT_ENCODING:
		/* Write me! */
		break;
	case JSP_ENTITY_XML_ENCODING:
		/* Write me! */
		break;
	case JSP_ENTITY_XML_VERSION:
		/* Write me! */
		break;
	default:
		return Node_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec Entity_props[] = {
	{ "publicId",		JSP_ENTITY_PUBLIC_ID,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "systemId",		JSP_ENTITY_SYSTEM_ID,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "notationName",	JSP_ENTITY_NOTATION_NAME,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "inputEncoding",	JSP_ENTITY_INPUT_ENCODING,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "xmlEncoding",	JSP_ENTITY_XML_ENCODING,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "xmlVersion",		JSP_ENTITY_XML_VERSION,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec Entity_funcs[] = {
	{ NULL }
};

const JSClass Entity_class = {
	"Entity",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	Entity_getProperty, Node_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

