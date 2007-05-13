#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLUListElement.h"

static JSBool
HTMLUListElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ULIST_ELEMENT_COMPACT:
		/* Write me! */
		break;
	case JSP_HTML_ULIST_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLUListElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ULIST_ELEMENT_COMPACT:
		/* Write me! */
		break;
	case JSP_HTML_ULIST_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLUListElement_props[] = {
	{ "compact",		JSP_HTML_ULIST_ELEMENT_COMPACT,	JSPROP_ENUMERATE },
	{ "type",		JSP_HTML_ULIST_ELEMENT_TYPE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSClass HTMLUListElement_class = {
	"HTMLUListElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLUListElement_getProperty, HTMLUListElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

