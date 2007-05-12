#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLImageElement.h"

static JSBool
HTMLImageElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IMAGE_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_ALT:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_HSPACE:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_IS_MAP:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_LONG_DESC:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_SRC:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_USE_MAP:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_VSPACE:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLImageElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IMAGE_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_ALT:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_HSPACE:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_IS_MAP:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_LONG_DESC:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_SRC:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_USE_MAP:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_VSPACE:
		/* Write me! */
		break;
	case JSP_HTML_IMAGE_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLImageElement_props[] = {
	{ "name",	JSP_HTML_IMAGE_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "align",	JSP_HTML_IMAGE_ELEMENT_ALIGN,		JSPROP_ENUMERATE },
	{ "alt",	JSP_HTML_IMAGE_ELEMENT_ALT,		JSPROP_ENUMERATE },
	{ "border",	JSP_HTML_IMAGE_ELEMENT_BORDER,		JSPROP_ENUMERATE },
	{ "height",	JSP_HTML_IMAGE_ELEMENT_HEIGHT,		JSPROP_ENUMERATE },
	{ "hspace",	JSP_HTML_IMAGE_ELEMENT_HSPACE,		JSPROP_ENUMERATE },
	{ "isMap",	JSP_HTML_IMAGE_ELEMENT_IS_MAP,		JSPROP_ENUMERATE },
	{ "longDesc",	JSP_HTML_IMAGE_ELEMENT_LONG_DESC,	JSPROP_ENUMERATE },
	{ "src",	JSP_HTML_IMAGE_ELEMENT_SRC,		JSPROP_ENUMERATE },
	{ "useMap",	JSP_HTML_IMAGE_ELEMENT_USE_MAP,		JSPROP_ENUMERATE },
	{ "vspace",	JSP_HTML_IMAGE_ELEMENT_VSPACE,		JSPROP_ENUMERATE },
	{ "width",	JSP_HTML_IMAGE_ELEMENT_WIDTH,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLImageElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLImageElement_class = {
	"HTMLImageElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLImageElement_getProperty, HTMLImageElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

