#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLAreaElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"

static JSBool
HTMLAreaElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_AREA_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_ALT:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_COORDS:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_HREF:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_NO_HREF:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_SHAPE:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_TARGET:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLAreaElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_AREA_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_ALT:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_COORDS:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_HREF:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_NO_HREF:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_SHAPE:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_AREA_ELEMENT_TARGET:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLAreaElement_props[] = {
	{ "accessKey",	JSP_HTML_AREA_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "alt",	JSP_HTML_AREA_ELEMENT_ALT,		JSPROP_ENUMERATE },
	{ "coords",	JSP_HTML_AREA_ELEMENT_COORDS,		JSPROP_ENUMERATE },
	{ "href",	JSP_HTML_AREA_ELEMENT_HREF,		JSPROP_ENUMERATE },
	{ "noHref",	JSP_HTML_AREA_ELEMENT_NO_HREF,		JSPROP_ENUMERATE },
	{ "shape",	JSP_HTML_AREA_ELEMENT_SHAPE,		JSPROP_ENUMERATE },
	{ "tabIndex",	JSP_HTML_AREA_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ "target",	JSP_HTML_AREA_ELEMENT_TARGET,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLAreaElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLAreaElement_class = {
	"HTMLAreaElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLAreaElement_getProperty, HTMLAreaElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

