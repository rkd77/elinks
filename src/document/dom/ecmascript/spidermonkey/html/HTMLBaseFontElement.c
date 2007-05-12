#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBaseFontElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"

static JSBool
HTMLBaseFontElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BASE_FONT_ELEMENT_COLOR:
		/* Write me! */
		break;
	case JSP_HTML_BASE_FONT_ELEMENT_FACE:
		/* Write me! */
		break;
	case JSP_HTML_BASE_FONT_ELEMENT_SIZE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLBaseFontElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BASE_FONT_ELEMENT_COLOR:
		/* Write me! */
		break;
	case JSP_HTML_BASE_FONT_ELEMENT_FACE:
		/* Write me! */
		break;
	case JSP_HTML_BASE_FONT_ELEMENT_SIZE:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLBaseFontElement_props[] = {
	{ "color",	JSP_HTML_BASE_FONT_ELEMENT_COLOR,	JSPROP_ENUMERATE },
	{ "face",	JSP_HTML_BASE_FONT_ELEMENT_FACE,	JSPROP_ENUMERATE },
	{ "size",	JSP_HTML_BASE_FONT_ELEMENT_SIZE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLBaseFontElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLBaseFontElement_class = {
	"HTMLBaseFontElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLBaseFontElement_getProperty, HTMLBaseFontElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

