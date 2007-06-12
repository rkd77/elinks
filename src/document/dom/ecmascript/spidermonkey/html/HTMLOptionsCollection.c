#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLCollection.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOptionsCollection.h"
#include "dom/node.h"

static JSBool
HTMLOptionsCollection_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node_list *list;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLOptionsCollection_class, NULL)))
		return JS_FALSE;

	list = JS_GetPrivate(ctx, obj);
	if (!list)
		return JS_FALSE;

	if (JSVAL_IS_STRING(id))
		return HTMLCollection_namedItem(ctx, obj, 1, &id, vp);

	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		case JSP_HTML_OPTIONS_COLLECTION_LENGTH:
			int_to_jsval(ctx, vp, list->size);
			break;
		default:
			return HTMLCollection_item(ctx, obj, 1, &id, vp);
			break;
		}
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLOptionsCollection_props[] = {
	{ "length",	JSP_HTML_OPTIONS_COLLECTION_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLOptionsCollection_funcs[] = {
	{ "item",	HTMLCollection_item,		1 },
	{ "namedItem",	HTMLCollection_namedItem,	1 },
	{ NULL }
};

const JSClass HTMLOptionsCollection_class = {
	"HTMLOptionsCollection",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLOptionsCollection_getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

