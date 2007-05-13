#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLMetaElement.h"

static JSBool
HTMLMetaElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_META_ELEMENT_CONTENT:
		/* Write me! */
		break;
	case JSP_HTML_META_ELEMENT_HTTP_EQUIV:
		/* Write me! */
		break;
	case JSP_HTML_META_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_META_ELEMENT_SCHEME:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLMetaElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_META_ELEMENT_CONTENT:
		/* Write me! */
		break;
	case JSP_HTML_META_ELEMENT_HTTP_EQUIV:
		/* Write me! */
		break;
	case JSP_HTML_META_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_META_ELEMENT_SCHEME:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLMetaElement_props[] = {
	{ "content",	JSP_HTML_META_ELEMENT_CONTENT,		JSPROP_ENUMERATE },
	{ "httpEquiv",	JSP_HTML_META_ELEMENT_HTTP_EQUIV,	JSPROP_ENUMERATE },
	{ "name",	JSP_HTML_META_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "scheme",	JSP_HTML_META_ELEMENT_SCHEME,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLMetaElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLMetaElement_class = {
	"HTMLMetaElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLMetaElement_getProperty, HTMLMetaElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

