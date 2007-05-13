#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLAnchorElement.h"

static JSBool
HTMLAnchorElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ANCHOR_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_CHARSET:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_COORDS:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_HREF:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_HREFLANG:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_REL:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_REV:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_SHAPE:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TARGET:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLAnchorElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ANCHOR_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_CHARSET:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_COORDS:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_HREF:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_HREFLANG:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_REL:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_REV:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_SHAPE:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TARGET:
		/* Write me! */
		break;
	case JSP_HTML_ANCHOR_ELEMENT_TYPE:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLAnchorElement_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLAnchorElement_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLAnchorElement_props[] = {
	{ "accessKey",	JSP_HTML_ANCHOR_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "charset",	JSP_HTML_ANCHOR_ELEMENT_CHARSET,	JSPROP_ENUMERATE },
	{ "coords",	JSP_HTML_ANCHOR_ELEMENT_COORDS,		JSPROP_ENUMERATE },
	{ "href",	JSP_HTML_ANCHOR_ELEMENT_HREF,		JSPROP_ENUMERATE },
	{ "hreflang",	JSP_HTML_ANCHOR_ELEMENT_HREFLANG,	JSPROP_ENUMERATE },
	{ "name",	JSP_HTML_ANCHOR_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "rel",	JSP_HTML_ANCHOR_ELEMENT_REL,		JSPROP_ENUMERATE },
	{ "rev",	JSP_HTML_ANCHOR_ELEMENT_REV,		JSPROP_ENUMERATE },
	{ "shape",	JSP_HTML_ANCHOR_ELEMENT_SHAPE,		JSPROP_ENUMERATE },
	{ "tabIndex",	JSP_HTML_ANCHOR_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ "target",	JSP_HTML_ANCHOR_ELEMENT_TARGET,		JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_ANCHOR_ELEMENT_TYPE,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLAnchorElement_funcs[] = {
	{ "blur",	HTMLAnchorElement_blur,	0 },
	{ "focus",	HTMLAnchorElement_focus,	0 },
	{ NULL }
};

const JSClass HTMLAnchorElement_class = {
	"HTMLAnchorElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLAnchorElement_getProperty, HTMLAnchorElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

