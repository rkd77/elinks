#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLIFrameElement.h"

static JSBool
HTMLIFrameElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IFRAME_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_FRAME_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_LONG_DESC:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_MARGIN_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_MARGIN_WIDTH:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_SCROLLING:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_SRC:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_WIDTH:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_CONTENT_DOCUMENT:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLIFrameElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IFRAME_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_FRAME_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_LONG_DESC:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_MARGIN_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_MARGIN_WIDTH:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_SCROLLING:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_SRC:
		/* Write me! */
		break;
	case JSP_HTML_IFRAME_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLIFrameElement_props[] = {
	{ "align",		JSP_HTML_IFRAME_ELEMENT_ALIGN,			JSPROP_ENUMERATE },
	{ "frameBorder",	JSP_HTML_IFRAME_ELEMENT_FRAME_BORDER,		JSPROP_ENUMERATE },
	{ "height",		JSP_HTML_IFRAME_ELEMENT_HEIGHT,			JSPROP_ENUMERATE },
	{ "longDesc",		JSP_HTML_IFRAME_ELEMENT_LONG_DESC,		JSPROP_ENUMERATE },
	{ "marginHeight",	JSP_HTML_IFRAME_ELEMENT_MARGIN_HEIGHT,		JSPROP_ENUMERATE },
	{ "marginWidth",	JSP_HTML_IFRAME_ELEMENT_MARGIN_WIDTH,		JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_IFRAME_ELEMENT_NAME,			JSPROP_ENUMERATE },
	{ "scrolling",		JSP_HTML_IFRAME_ELEMENT_SCROLLING,		JSPROP_ENUMERATE },
	{ "src",		JSP_HTML_IFRAME_ELEMENT_SRC,			JSPROP_ENUMERATE },
	{ "width",		JSP_HTML_IFRAME_ELEMENT_WIDTH,			JSPROP_ENUMERATE },
	{ "contentDocument",	JSP_HTML_IFRAME_ELEMENT_CONTENT_DOCUMENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLIFrameElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLIFrameElement_class = {
	"HTMLIFrameElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLIFrameElement_getProperty, HTMLIFrameElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

