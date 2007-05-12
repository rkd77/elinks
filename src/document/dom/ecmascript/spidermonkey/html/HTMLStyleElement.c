#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLStyleElement.h"

static JSBool
HTMLStyleElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_STYLE_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_STYLE_ELEMENT_MEDIA:
		/* Write me! */
		break;
	case JSP_HTML_STYLE_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLStyleElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_STYLE_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_STYLE_ELEMENT_MEDIA:
		/* Write me! */
		break;
	case JSP_HTML_STYLE_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLStyleElement_props[] = {
	{ "disabled",	JSP_HTML_STYLE_ELEMENT_DISABLED,JSPROP_ENUMERATE },
	{ "media",	JSP_HTML_STYLE_ELEMENT_MEDIA,	JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_STYLE_ELEMENT_TYPE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLStyleElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLStyleElement_class = {
	"HTMLStyleElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLStyleElement_getProperty, HTMLStyleElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

