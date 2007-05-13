#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/DocumentType.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"

static JSBool
DocumentType_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOCUMENT_TYPE_NAME:
		/* Write me! */
		break;
	case JSP_DOCUMENT_TYPE_ENTITIES:
		/* Write me! */
		break;
	case JSP_DOCUMENT_TYPE_NOTATIONS:
		/* Write me! */
		break;
	case JSP_DOCUMENT_TYPE_PUBLIC_ID:
		/* Write me! */
		break;
	case JSP_DOCUMENT_TYPE_SYSTEM_ID:
		/* Write me! */
		break;
	case JSP_DOCUMENT_TYPE_INTERNAL_SUBSET:
		/* Write me! */
		break;
	default:
		return Node_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec DocumentType_props[] = {
	{ "name",		JSP_DOCUMENT_TYPE_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "entities",		JSP_DOCUMENT_TYPE_ENTITIES,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "notations",		JSP_DOCUMENT_TYPE_NOTATIONS,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "publicId",		JSP_DOCUMENT_TYPE_PUBLIC_ID,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "systemId",		JSP_DOCUMENT_TYPE_SYSTEM_ID,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "internalSubset",	JSP_DOCUMENT_TYPE_INTERNAL_SUBSET,JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec DocumentType_funcs[] = {
	{ NULL }
};

const JSClass DocumentType_class = {
	"DocumentType",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	DocumentType_getProperty, Node_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

