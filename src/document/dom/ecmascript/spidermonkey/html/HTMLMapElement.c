#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLMapElement.h"

static JSBool
HTMLMapElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_MAP_ELEMENT_AREAS:
		/* Write me! */
		break;
	case JSP_HTML_MAP_ELEMENT_NAME:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLMapElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_MAP_ELEMENT_NAME:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLMapElement_props[] = {
	{ "areas",	JSP_HTML_MAP_ELEMENT_AREAS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "name",	JSP_HTML_MAP_ELEMENT_NAME,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLMapElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLMapElement_class = {
	"HTMLMapElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLMapElement_getProperty, HTMLMapElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

