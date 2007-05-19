#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "dom/node.h"


static void
Node_get_node_value(struct dom_node *node, JSContext *ctx, JSObject *obj, jsval *vp)
{
	switch (node->type) {
	case DOM_NODE_DOCUMENT:
	case DOM_NODE_DOCUMENT_FRAGMENT:
	case DOM_NODE_DOCUMENT_TYPE:
	case DOM_NODE_ELEMENT:
	case DOM_NODE_ENTITY:
	case DOM_NODE_ENTITY_REFERENCE:
	case DOM_NODE_NOTATION:
	default:
		undef_to_jsval(ctx, vp);
		break;
	case DOM_NODE_ATTRIBUTE:
		JS_GetProperty(ctx, obj, "value", vp);
		break;
	case DOM_NODE_TEXT:
	case DOM_NODE_CDATA_SECTION:
	case DOM_NODE_COMMENT:
	case DOM_NODE_PROCESSING_INSTRUCTION:
		JS_GetProperty(ctx, obj, "data", vp);
		break;
	}
}

static void
Node_set_node_value(struct dom_node *node, JSContext *ctx, JSObject *obj, jsval *vp)
{
	switch (node->type) {
	case DOM_NODE_DOCUMENT:
	case DOM_NODE_DOCUMENT_FRAGMENT:
	case DOM_NODE_DOCUMENT_TYPE:
	case DOM_NODE_ELEMENT:
	case DOM_NODE_ENTITY:
	case DOM_NODE_ENTITY_REFERENCE:
	case DOM_NODE_NOTATION:
	default:
		break;
	case DOM_NODE_ATTRIBUTE:
		JS_SetProperty(ctx, obj, "value", vp);
		break;
	case DOM_NODE_TEXT:
	case DOM_NODE_CDATA_SECTION:
	case DOM_NODE_COMMENT:
	case DOM_NODE_PROCESSING_INSTRUCTION:
		JS_SetProperty(ctx, obj, "data", vp);
		break;
	}
}

static void
Node_get_node_name(struct dom_node *node, JSContext *ctx, JSObject *obj, jsval *vp)
{
	switch (node->type) {
	case DOM_NODE_DOCUMENT:
		string_to_jsval(ctx, vp, "#document");
		break;
	case DOM_NODE_DOCUMENT_FRAGMENT:
		string_to_jsval(ctx, vp, "#document-fragment");
		break;
	case DOM_NODE_DOCUMENT_TYPE:
		JS_GetProperty(ctx, obj, "name", vp);
		break;
	case DOM_NODE_ELEMENT:
		JS_GetProperty(ctx, obj, "tagName", vp);
		break;
	case DOM_NODE_ENTITY:
	case DOM_NODE_ENTITY_REFERENCE:
	case DOM_NODE_NOTATION:
	default:
		/* I don't know. */
		undef_to_jsval(ctx, vp);
		break;
	case DOM_NODE_ATTRIBUTE:
		JS_GetProperty(ctx, obj, "name", vp);
		break;
	case DOM_NODE_TEXT:
		string_to_jsval(ctx, vp, "#text");
		break;
	case DOM_NODE_CDATA_SECTION:
		string_to_jsval(ctx, vp, "#cdata-section");
		break;
	case DOM_NODE_COMMENT:
		string_to_jsval(ctx, vp, "#comment");
		break;
	case DOM_NODE_PROCESSING_INSTRUCTION:
		JS_GetProperty(ctx, obj, "target", vp);
		break;
	}
}

static void
Node_get_text_content(struct dom_node *node, JSContext *ctx, JSObject *obj, jsval *vp)
{
	switch (node->type) {
	case DOM_NODE_TEXT:
	case DOM_NODE_CDATA_SECTION:
	case DOM_NODE_COMMENT:
		JS_GetProperty(ctx, obj, "nodeValue", vp);
		break;
	case DOM_NODE_DOCUMENT:
	case DOM_NODE_DOCUMENT_TYPE:
	case DOM_NODE_NOTATION:
	default:
		undef_to_jsval(ctx, vp);
		break;
	case DOM_NODE_ELEMENT:
	case DOM_NODE_ATTRIBUTE:
	case DOM_NODE_ENTITY:
	case DOM_NODE_ENTITY_REFERENCE:
	case DOM_NODE_DOCUMENT_FRAGMENT:
		/* Write me! */
		break;
	}
}

JSBool
Node_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct dom_node_list *ch = NULL;
	JSObject *ret = NULL;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&Node_class, NULL)))
		return JS_FALSE;
	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_NODE_NODE_NAME:
		Node_get_node_name(node, ctx, obj, vp);
		break;
	case JSP_NODE_NODE_VALUE:
		Node_get_node_value(node, ctx, obj, vp);
		break;
	case JSP_NODE_NODE_TYPE:
		int_to_jsval(ctx, vp, node->type);
		break;
	case JSP_NODE_PARENT_NODE:
		node = node->parent;
		if (node)
			ret = node->ecmascript_obj;
		object_to_jsval(ctx, vp, ret);
		break;
	case JSP_NODE_CHILD_NODES:
		if (node->type == DOM_NODE_ELEMENT)
			ch = node->data.element.children;
		else if (node->type == DOM_NODE_DOCUMENT)
			ch = node->data.document.children;

		if (ch)
			ret = ch->ecmascript_obj;
		object_to_jsval(ctx, vp, ret);
		break;
	case JSP_NODE_FIRST_CHILD:
		if (node->type == DOM_NODE_ELEMENT)
			ch = node->data.element.children;
		else if (node->type == DOM_NODE_DOCUMENT)
			ch = node->data.document.children;

		if (ch && (ch->size > 0)) {
			struct dom_node *child = ch->entries[0];

			if (child)
				ret = child->ecmascript_obj;
		}
		object_to_jsval(ctx, vp, ret);
		break;
	case JSP_NODE_LAST_CHILD:
		if (node->type == DOM_NODE_ELEMENT)
			ch = node->data.element.children;
		else if (node->type == DOM_NODE_DOCUMENT)
			ch = node->data.document.children;

		if (ch && (ch->size > 0)) {
			struct dom_node *child = ch->entries[ch->size - 1];

			if (child)
				ret = child->ecmascript_obj;
		}
		object_to_jsval(ctx, vp, ret);
		break;
	case JSP_NODE_PREVIOUS_SIBLING:
		node = get_dom_node_prev(node);
		if (node)
			ret = node->ecmascript_obj;
		object_to_jsval(ctx, vp, ret);
		break;
	case JSP_NODE_NEXT_SIBLING:
		node = get_dom_node_next(node);
		if (node)
			ret = node->ecmascript_obj;
		object_to_jsval(ctx, vp, ret);
		break;
	case JSP_NODE_ATTRIBUTES:
		if (node->type == DOM_NODE_ELEMENT) {
			struct dom_node_list *attrs = node->data.element.map;

			if (attrs)
				ret = attrs->ecmascript_obj;
		}
		object_to_jsval(ctx, vp, ret);
		break;
	case JSP_NODE_OWNER_DOCUMENT:
		node = get_dom_root_node(node);
		if (node)
			ret = node->ecmascript_obj;
		object_to_jsval(ctx, vp, ret);
		break;
	case JSP_NODE_NAMESPACE_URI:
		if (node->type == DOM_NODE_ATTRIBUTE || node->type == DOM_NODE_ELEMENT) {
			/* Write me! */
			break;
		}
		undef_to_jsval(ctx, vp);
		break;
	case JSP_NODE_PREFIX:
		if (node->type == DOM_NODE_ATTRIBUTE || node->type == DOM_NODE_ELEMENT) {
			/* Write me! */
			break;
		}
		undef_to_jsval(ctx, vp);
		break;
	case JSP_NODE_LOCAL_NAME:
		if (node->type == DOM_NODE_ATTRIBUTE || node->type == DOM_NODE_ELEMENT) {
			/* Write me! */
			break;
		}
		undef_to_jsval(ctx, vp);
		break;
	case JSP_NODE_BASE_URI:
		/* Write me! */
		break;
	case JSP_NODE_TEXT_CONTENT:
		Node_get_text_content(node, ctx, obj, vp);
		break;
	default:
		break;
	}
	return JS_TRUE;
}

JSBool
Node_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&Node_class, NULL)))
		return JS_FALSE;
	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_NODE_NODE_VALUE:
		Node_set_node_value(node, ctx, obj, vp);
		break;
	case JSP_NODE_PREFIX:
		if (node->type == DOM_NODE_ATTRIBUTE || node->type == DOM_NODE_ELEMENT) {
			/* Write me! */
			break;
		}
		break;
	case JSP_NODE_TEXT_CONTENT:
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

void
Node_finalize(JSContext *ctx, JSObject *obj)
{
	assert(ctx && obj);

	struct dom_node *node = JS_GetPrivate(ctx, obj);

	if (node) {
		node->ecmascript_obj = NULL;
		JS_SetPrivate(ctx, obj, NULL);
	}
}

const JSPropertySpec Node_props[] = {
	{ "nodeName",		JSP_NODE_NODE_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "nodeValue",		JSP_NODE_NODE_VALUE,		JSPROP_ENUMERATE },
	{ "nodeType",		JSP_NODE_NODE_TYPE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "parentNode",		JSP_NODE_PARENT_NODE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "childNodes",		JSP_NODE_CHILD_NODES,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "firstChild",		JSP_NODE_FIRST_CHILD,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "lastChild",		JSP_NODE_LAST_CHILD,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "previousSibling",	JSP_NODE_PREVIOUS_SIBLING,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "nextSibling",	JSP_NODE_NEXT_SIBLING,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "attributes",		JSP_NODE_ATTRIBUTES,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "ownerDocument",	JSP_NODE_OWNER_DOCUMENT,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "namespaceURI",	JSP_NODE_NAMESPACE_URI,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "prefix"	,	JSP_NODE_PREFIX,			JSPROP_ENUMERATE },
	{ "localName",		JSP_NODE_LOCAL_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "baseURI",		JSP_NODE_BASE_URI,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "textContent",	JSP_NODE_TEXT_CONTENT,		JSPROP_ENUMERATE },
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
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

