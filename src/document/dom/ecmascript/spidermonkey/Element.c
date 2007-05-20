#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Attr.h"
#include "document/dom/ecmascript/spidermonkey/Element.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "dom/sgml/html/html.h"
#include "dom/string.h"

JSBool
Element_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	uint16_t type;
	unsigned char *elem_name;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&Element_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case JSP_ELEMENT_TAG_NAME:
		type = node->data.element.type;
		if (type > 0 && type < HTML_ELEMENTS) {
			elem_name = sgml_html_info.elements[type].string.string;
			string_to_jsval(ctx, vp, elem_name);
		}
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
	unsigned char *attr;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&Element_class, NULL))
	 || argc != 1)
		return JS_FALSE;

	/* This will work only with HTML. --witekfl */
	attr = jsval_to_string(ctx, &argv[0]);
	JS_GetProperty(ctx, obj, attr, rval);
	return JS_TRUE;
}

static JSBool
Element_setAttribute(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	unsigned char *attr;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&Element_class, NULL))
	 || argc != 2)
		return JS_FALSE;

	/* This will work only with HTML. --witekfl */
	attr = jsval_to_string(ctx, &argv[0]);
	JS_SetProperty(ctx, obj, attr, &argv[1]);
	return JS_TRUE;
}

static JSBool
Element_removeAttribute(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	unsigned char *attr;
	
	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&Element_class, NULL))
	 || argc != 1)
		return JS_FALSE;

	attr = jsval_to_string(ctx, &argv[0]);
	JS_SetProperty(ctx, obj, attr, NULL); /* Special case. */
	return JS_TRUE;
}

static JSBool
Element_getAttributeNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	struct dom_node *node, *searched;
	struct dom_node_list *list;
	struct dom_string name;
	unsigned char *attr;
	JSObject *ret = NULL;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&Element_class, NULL))
	 || argc != 1)
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;

	attr = jsval_to_string(ctx, &argv[0]);
	if (!attr) {
		undef_to_jsval(ctx, rval);
		return JS_TRUE;
	}
	list = node->data.element.map;
	name.string = attr;
	name.length = strlen(attr);
	searched = get_dom_node_map_entry(list, DOM_NODE_ATTRIBUTE, 0, &name);
	if (searched)
		ret = searched->ecmascript_obj;
	object_to_jsval(ctx, rval, ret);
	return JS_TRUE;
}

static JSBool
Element_setAttributeNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	jsval v;
	struct dom_node *node, *searched;
	struct dom_node_list *list;
	struct dom_string name;
	JSObject *attr, *ret = NULL;
	unsigned char *data;
 
	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&Element_class, NULL))
	 || argc != 1)
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;

	attr = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(ctx, attr, (JSClass *)&Attr_class, NULL))
		return JS_FALSE;

	JS_GetProperty(ctx, attr, "name", &v);
	data = jsval_to_string(ctx, &v);
	if (data) {
		name.string = data;
		name.length = strlen(data);
		list = node->data.element.map;
		searched = get_dom_node_map_entry(list, DOM_NODE_ATTRIBUTE, 0, &name);
		if (searched)
			ret = searched->ecmascript_obj;
	}
	JS_GetProperty(ctx, attr, "value", &v);
	Element_setAttribute(ctx, obj, 1, &v, NULL);
	object_to_jsval(ctx, rval, ret);
	return JS_TRUE;
}

static JSBool
Element_removeAttributeNode(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* This is probably wrong! */
	jsval v;
	struct dom_node *node, *searched;
	struct dom_node_list *list;
	struct dom_string name;
	JSObject *attr, *ret = NULL;
	unsigned char *data;
 
	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&Element_class, NULL))
	 || argc != 1)
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;

	attr = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(ctx, attr, (JSClass *)&Attr_class, NULL))
		return JS_FALSE;

	JS_GetProperty(ctx, attr, "name", &v);
	data = jsval_to_string(ctx, &v);
	if (data) {
		name.string = data;
		name.length = strlen(data);
		list = node->data.element.map;
		searched = get_dom_node_map_entry(list, DOM_NODE_ATTRIBUTE, 0, &name);
		if (searched)
			ret = searched->ecmascript_obj;
	}
	undef_to_jsval(ctx, &v);
	Element_setAttribute(ctx, obj, 1, &v, NULL);
	object_to_jsval(ctx, rval, ret);
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
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

