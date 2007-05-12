#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBaseElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"

static JSBool
HTMLBaseElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BASE_ELEMENT_HREF:
		/* Write me! */
		break;
	case JSP_HTML_BASE_ELEMENT_TARGET:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLBaseElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BASE_ELEMENT_HREF:
		/* Write me! */
		break;
	case JSP_HTML_BASE_ELEMENT_TARGET:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLBaseElement_props[] = {
	{ "href",	JSP_HTML_BASE_ELEMENT_HREF,	JSPROP_ENUMERATE },
	{ "target",	JSP_HTML_BASE_ELEMENT_TARGET,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLBaseElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLBaseElement_class = {
	"HTMLBaseElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLBaseElement_getProperty, HTMLBaseElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

