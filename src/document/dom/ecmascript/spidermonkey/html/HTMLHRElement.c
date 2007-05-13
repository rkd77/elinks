#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLHRElement.h"

static JSBool
HTMLHRElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_HR_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_HR_ELEMENT_NO_SHADE:
		/* Write me! */
		break;
	case JSP_HTML_HR_ELEMENT_SIZE:
		/* Write me! */
		break;
	case JSP_HTML_HR_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLHRElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_HR_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_HR_ELEMENT_NO_SHADE:
		/* Write me! */
		break;
	case JSP_HTML_HR_ELEMENT_SIZE:
		/* Write me! */
		break;
	case JSP_HTML_HR_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLHRElement_props[] = {
	{ "align",	JSP_HTML_HR_ELEMENT_ALIGN,	JSPROP_ENUMERATE },
	{ "noShade",	JSP_HTML_HR_ELEMENT_NO_SHADE,	JSPROP_ENUMERATE },
	{ "size",	JSP_HTML_HR_ELEMENT_SIZE,	JSPROP_ENUMERATE },
	{ "width",	JSP_HTML_HR_ELEMENT_WIDTH,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLHRElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLHRElement_class = {
	"HTMLHRElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLHRElement_getProperty, HTMLHRElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

