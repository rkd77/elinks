#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Element.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"

JSBool
Element_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_ELEMENT_TAG_NAME:
		/* Write me! */
		break;
	case JSP_ELEMENT_SCHEMA_TYPE_INFO:
		/* Write me! */
		break;
	default:
		return Node_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
Element_getAttribute(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_setAttribute(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_removeAttribute(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_getAttributeNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_setAttributeNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_removeAttributeNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_getElementsByTagName(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_getAttributeNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_setAttributeNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_removeAttributeNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}
static JSBool
Element_getAttributeNodeNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_setAttributeNodeNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_getElementsByTagNameNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_hasAttribute(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_hasAttributeNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_setIdAttribute(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_setIdAttributeNS(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Element_setIdAttributeNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec Element_props[] = {
	{ "tagName",		JSP_ELEMENT_TAG_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "schemaTypeInfo",	JSP_ELEMENT_SCHEMA_TYPE_INFO,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec Element_funcs[] = {
	{ "getAttribute",		Element_getAttribute,		1 },
	{ "setAttribute",		Element_setAttribute,		2 },
	{ "removeAttribute",		Element_removeAttribute,	1 },
	{ "getAttributeNode",		Element_getAttributeNode,	1 },
	{ "setAttributeNode",		Element_setAttributeNode,	1 },
	{ "removeAttributeNode",	Element_removeAttributeNode,	1 },
	{ "getElementsByTagName",	Element_getElementsByTagName,	1 },
	{ "getAttributeNS",		Element_getAttributeNS,		2 },
	{ "setAttributeNS",		Element_setAttributeNS,		3 },
	{ "removeAttributeNS",		Element_removeAttributeNS,	2 },
	{ "getAttributeNodeNS",		Element_getAttributeNodeNS,	2 },
	{ "setAttributeNodeNS",		Element_setAttributeNodeNS,	1 },
	{ "getElementsByTagNameNS",	Element_getElementsByTagNameNS,	2 },
	{ "hasAttribute",		Element_hasAttribute,		1 },
	{ "hasAttributeNS",		Element_hasAttributeNS,		2 },
	{ "setIdAttribute",		Element_setIdAttribute,		2 },
	{ "setIdAttributeNS",		Element_setIdAttributeNS,	3 },
	{ "setIdAttributeNode",		Element_setIdAttributeNode,	2 },
	{ NULL }
};

const JSClass Element_class = {
	"Element",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	Element_getProperty, Node_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

