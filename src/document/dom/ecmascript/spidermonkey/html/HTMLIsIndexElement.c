#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLIsIndexElement.h"

static JSBool
HTMLIsIndexElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IS_INDEX_ELEMENT_FORM:
		/* Write me! */
		break;
	case JSP_HTML_IS_INDEX_ELEMENT_PROMPT:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLIsIndexElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IS_INDEX_ELEMENT_PROMPT:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLIsIndexElement_props[] = {
	{ "form",	JSP_HTML_IS_INDEX_ELEMENT_FORM,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "prompt",	JSP_HTML_IS_INDEX_ELEMENT_PROMPT,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLIsIndexElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLIsIndexElement_class = {
	"HTMLIsIndexElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLIsIndexElement_getProperty, HTMLIsIndexElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

