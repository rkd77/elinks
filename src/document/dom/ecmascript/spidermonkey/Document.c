#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Document.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"

JSBool
Document_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOCUMENT_DOCTYPE:
		/* Write me! */
		break;
	case JSP_DOCUMENT_IMPLEMENTATION:
		/* Write me! */
		break;
	case JSP_DOCUMENT_DOCUMENT_ELEMENT:
		/* Write me! */
		break;
	case JSP_DOCUMENT_INPUT_ENCODING:
		/* Write me! */
		break;
	case JSP_DOCUMENT_XML_ENCODING:
		/* Write me! */
		break;
	case JSP_DOCUMENT_XML_STANDALONE:
		/* Write me! */
		break;
	case JSP_DOCUMENT_XML_VERSION:
		/* Write me! */
		break;
	case JSP_DOCUMENT_STRICT_ERROR_CHECKING:
		/* Write me! */
		break;
	case JSP_DOCUMENT_DOCUMENT_URI:
		/* Write me! */
		break;
	case JSP_DOCUMENT_DOM_CONFIG:
		/* Write me! */
		break;
	default:
		return Node_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

JSBool
Document_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOCUMENT_XML_STANDALONE:
		/* Write me! */
		break;
	case JSP_DOCUMENT_XML_VERSION:
		/* Write me! */
		break;
	case JSP_DOCUMENT_STRICT_ERROR_CHECKING:
		/* Write me! */
		break;
	case JSP_DOCUMENT_DOCUMENT_URI:
		/* Write me! */
		break;
	default:
		return Node_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
Document_createElement(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_createDocumentFragment(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_createTextNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_createComment(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_createCDATASection(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_createProcessingInstruction(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_createAttribute(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_createEntityReference(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_getElementsByTagName(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_importNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_createElementNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_createAttributeNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_getElementsByTagNameNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_getElementById(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_adoptNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_normalizeDocument(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Document_renameNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec Document_props[] = {
	{ "doctype",		JSP_DOCUMENT_DOCTYPE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "implementation",	JSP_DOCUMENT_IMPLEMENTATION,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "documentElement",	JSP_DOCUMENT_DOCUMENT_ELEMENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "inputEncoding",	JSP_DOCUMENT_INPUT_ENCODING,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "xmlEncoding",	JSP_DOCUMENT_XML_ENCODING,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "xmlStandalone",	JSP_DOCUMENT_XML_STANDALONE,		JSPROP_ENUMERATE },
	{ "xmlVersion",		JSP_DOCUMENT_XML_VERSION,		JSPROP_ENUMERATE },
	{ "strictErrorChecking",JSP_DOCUMENT_STRICT_ERROR_CHECKING,	JSPROP_ENUMERATE },
	{ "documentURI",	JSP_DOCUMENT_DOCUMENT_URI,		JSPROP_ENUMERATE },
	{ "domConfig",		JSP_DOCUMENT_DOM_CONFIG,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec Document_funcs[] = {
	{ "createElement",		Document_createElement,			1 },
	{ "createDocumentFragment",	Document_createDocumentFragment,	0 },
	{ "createTextNode",		Document_createTextNode,		1 },
	{ "createComment",		Document_createComment,			1 },
	{ "createCDATASection",		Document_createCDATASection,		1 },
	{ "createProcessingInstruction",Document_createProcessingInstruction,	2 },
	{ "createAttribute",		Document_createAttribute,		1 },
	{ "createEntityReference",	Document_createEntityReference,		1 },
	{ "getElementsByTagName",	Document_getElementsByTagName,		1 },
	{ "importNode",			Document_importNode,			2 },
	{ "createElementNS",		Document_createElementNS,		2 },
	{ "createAttributeNS",		Document_createAttributeNS,		2 },
	{ "getElementsByTagNameNS",	Document_getElementsByTagNameNS,	2 },
	{ "getElementById",		Document_getElementById,		1 },
	{ "adoptNode",			Document_adoptNode,			1 },
	{ "normalizeDocument",		Document_normalizeDocument,		0 },
	{ "renameNode",			Document_renameNode,			3 },
	{ NULL }
};

const JSClass Document_class = {
	"Document",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	Document_getProperty, Document_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

