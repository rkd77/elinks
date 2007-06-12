#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLCollection.h"
#include "dom/node.h"

static JSBool
HTMLCollection_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	struct dom_node_list *list;
	int ind;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLCollection_class, NULL)))
		return JS_FALSE;

	list = JS_GetPrivate(ctx, obj);
	if (!list)
		return JS_FALSE;

	if (!JS_ValueToInt32(ctx, argv[0], &ind))
		return JS_FALSE;

	if (ind >= list->size)
		undef_to_jsval(ctx, rval);
	else {
		struct dom_node *node = list->entries[ind];

		object_to_jsval(ctx, rval, node->ecmascript_obj);
	}
	return JS_TRUE;
}

static JSBool
HTMLCollection_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	struct dom_node_list *list;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLCollection_class, NULL)))
		return JS_FALSE;

	list = JS_GetPrivate(ctx, obj);
	if (!list)
		return JS_FALSE;


	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLCollection_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node_list *list;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLCollection_class, NULL)))
		return JS_FALSE;

	list = JS_GetPrivate(ctx, obj);
	if (!list)
		return JS_FALSE;

	if (JSVAL_IS_STRING(id))
		return HTMLCollection_namedItem(ctx, obj, 1, &id, vp);

	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		case JSP_HTML_COLLECTION_LENGTH:
			int_to_jsval(ctx, vp, list->size);
			break;
		default:
			return HTMLCollection_item(ctx, obj, 1, &id, vp);
			break;
		}
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLCollection_props[] = {
	{ "length",	JSP_HTML_COLLECTION_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLCollection_funcs[] = {
	{ "item",	HTMLCollection_item,		1 },
	{ "namedItem",	HTMLCollection_namedItem,	1 },
	{ NULL }
};

const JSClass HTMLCollection_class = {
	"HTMLCollection",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLCollection_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

