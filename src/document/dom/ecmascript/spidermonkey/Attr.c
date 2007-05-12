#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Attr.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"

static JSBool
Attr_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_ATTR_NAME:
		/* Write me! */
		break;
	case JSP_ATTR_SPECIFIED:
		/* Write me! */
		break;
	case JSP_ATTR_VALUE:
		/* Write me! */
		break;
	case JSP_ATTR_OWNER_ELEMENT:
		/* Write me! */
		break;
	case JSP_ATTR_SCHEMA_TYPE_INFO:
		/* Write me! */
		break;
	case JSP_ATTR_IS_ID:
		/* Write me! */
		break;
	default:
		return Node_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
Attr_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_ATTR_VALUE:
		/* Write me! */
		break;
	default:
		return Node_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec Attr_props[] = {
	{ "name",		JSP_ATTR_NAME,			JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "specified",		JSP_ATTR_SPECIFIED,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "value",		JSP_ATTR_VALUE,			JSPROP_ENUMERATE },
	{ "ownerElement",	JSP_ATTR_OWNER_ELEMENT,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "schemaTypeInfo",	JSP_ATTR_SCHEMA_TYPE_INFO,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "isId",		JSP_ATTR_IS_ID,			JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSClass Attr_class = {
	"Attr",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	Attr_getProperty, Attr_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

