#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"

JSBool
Node_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_NODE_NAME:
		/* Write me! */
		break;
	case JSP_DOM_NODE_VALUE:
		/* Write me! */
		break;
	case JSP_DOM_NODE_TYPE:
		/* Write me! */
		break;
	case JSP_DOM_PARENT_NODE:
		/* Write me! */
		break;
	case JSP_DOM_CHILD_NODES:
		/* Write me! */
		break;
	case JSP_DOM_FIRST_CHILD:
		/* Write me! */
		break;
	case JSP_DOM_LAST_CHILD:
		/* Write me! */
		break;
	case JSP_DOM_PREVIOUS_SIBLING:
		/* Write me! */
		break;
	case JSP_DOM_NEXT_SIBLING:
		/* Write me! */
		break;
	case JSP_DOM_ATTRIBUTES:
		/* Write me! */
		break;
	case JSP_DOM_OWNER_DOCUMENT:
		/* Write me! */
		break;
	case JSP_DOM_NAMESPACE_URI:
		/* Write me! */
		break;
	case JSP_DOM_PREFIX:
		/* Write me! */
		break;
	case JSP_DOM_LOCAL_NAME:
		/* Write me! */
		break;
	case JSP_DOM_BASE_URI:
		/* Write me! */
		break;
	case JSP_DOM_TEXT_CONTENT:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

JSBool
Node_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_DOM_NODE_VALUE:
		/* Write me! */
		break;
	case JSP_DOM_PREFIX:
		/* Write me! */
		break;
	case JSP_DOM_TEXT_CONTENT:
		/* Write me! */
		break;
	default:
		break;
	}
	return JS_TRUE;
}

static JSBool
Node_insertBefore(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_replaceChild(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_removeChild(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_appendChild(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_hasChildNodes(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_cloneNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_normalize(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_isSupported(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_hasAttributes(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_compareDocumentPosition(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_isSameNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_lookupPrefix(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_isDefaultNamespace(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_lookupNamespaceURI(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_isEqualNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_getFeature(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_setUserData(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
Node_getUserData(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec Node_props[] = {
	{ "nodeName",		JSP_DOM_NODE_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "nodeValue",		JSP_DOM_NODE_VALUE,		JSPROP_ENUMERATE },
	{ "nodeType",		JSP_DOM_NODE_TYPE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "parentNode",		JSP_DOM_PARENT_NODE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "childNodes",		JSP_DOM_CHILD_NODES,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "firstChild",		JSP_DOM_FIRST_CHILD,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "lastChild",		JSP_DOM_LAST_CHILD,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "previousSibling",	JSP_DOM_PREVIOUS_SIBLING,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "nextSibling",	JSP_DOM_NEXT_SIBLING,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "attributes",		JSP_DOM_ATTRIBUTES,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "ownerDocument",	JSP_DOM_OWNER_DOCUMENT,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "namespaceURI",	JSP_DOM_NAMESPACE_URI,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "prefix"	,	JSP_DOM_PREFIX,			JSPROP_ENUMERATE },
	{ "localName",		JSP_DOM_LOCAL_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "baseURI",		JSP_DOM_BASE_URI,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "textContent",	JSP_DOM_TEXT_CONTENT,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec Node_funcs[] = {
	{ "insertBefore",		Node_insertBefore,		2 },
	{ "replaceChild",		Node_replaceChild,		2 },
	{ "removeChild",		Node_removeChild,		1 },
	{ "appendChild",		Node_appendChild,		1 },
	{ "hasChildNodes",		Node_hasChildNodes,		0 },
	{ "cloneNode",			Node_cloneNode,			1 },
	{ "normalize",			Node_normalize,			0 },
	{ "isSupported",		Node_isSupported,		2 },
	{ "hasAttributes",		Node_hasAttributes,		0 },
	{ "compareDocumentPosition",	Node_compareDocumentPosition,	1 },
	{ "isSameNode",			Node_isSameNode,		1 },
	{ "lookupPrefix",		Node_lookupPrefix,		1 },
	{ "isDefaultNamespace",		Node_isDefaultNamespace,	1 },
	{ "lookupNamespaceURI",		Node_lookupNamespaceURI,	1 },
	{ "isEqualNode",		Node_isEqualNode,		1 },
	{ "getFeature",			Node_getFeature,		2 },
	{ "setUserData",		Node_setUserData,		3 },
	{ "getUserData",		Node_getUserData,		1 },
	{ NULL }
};

const JSClass Node_class = {
	"Node",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	Node_getProperty, Node_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

