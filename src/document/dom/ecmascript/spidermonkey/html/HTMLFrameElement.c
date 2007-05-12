#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFrameElement.h"

static JSBool
HTMLFrameElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FRAME_ELEMENT_FRAME_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_LONG_DESC:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_MARGIN_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_MARGIN_WIDTH:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_NO_RESIZE:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_SCROLLING:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_SRC:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_CONTENT_DOCUMENT:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLFrameElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FRAME_ELEMENT_FRAME_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_LONG_DESC:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_MARGIN_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_MARGIN_WIDTH:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_NO_RESIZE:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_SCROLLING:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_ELEMENT_SRC:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLFrameElement_props[] = {
	{ "frameBorder",	JSP_HTML_FRAME_ELEMENT_FRAME_BORDER,	JSPROP_ENUMERATE },
	{ "longDesc",		JSP_HTML_FRAME_ELEMENT_LONG_DESC,	JSPROP_ENUMERATE },
	{ "marginHeight",	JSP_HTML_FRAME_ELEMENT_MARGIN_HEIGHT,	JSPROP_ENUMERATE },
	{ "marginWidth",	JSP_HTML_FRAME_ELEMENT_MARGIN_WIDTH,	JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_FRAME_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "noResize",		JSP_HTML_FRAME_ELEMENT_NO_RESIZE,	JSPROP_ENUMERATE },
	{ "scrolling",		JSP_HTML_FRAME_ELEMENT_SCROLLING,	JSPROP_ENUMERATE },
	{ "src",		JSP_HTML_FRAME_ELEMENT_SRC,		JSPROP_ENUMERATE },
	{ "contentDocument",	JSP_HTML_FRAME_ELEMENT_CONTENT_DOCUMENT,JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLFrameElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLFrameElement_class = {
	"HTMLFrameElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLFrameElement_getProperty, HTMLFrameElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

