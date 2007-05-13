#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFrameSetElement.h"

static JSBool
HTMLFrameSetElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FRAME_SET_ELEMENT_COLS:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_SET_ELEMENT_ROWS:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLFrameSetElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FRAME_SET_ELEMENT_COLS:
		/* Write me! */
		break;
	case JSP_HTML_FRAME_SET_ELEMENT_ROWS:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLFrameSetElement_props[] = {
	{ "cols",	JSP_HTML_FRAME_SET_ELEMENT_COLS,	JSPROP_ENUMERATE },
	{ "rows",	JSP_HTML_FRAME_SET_ELEMENT_ROWS,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLFrameSetElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLFrameSetElement_class = {
	"HTMLFrameSetElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLFrameSetElement_getProperty, HTMLFrameSetElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

