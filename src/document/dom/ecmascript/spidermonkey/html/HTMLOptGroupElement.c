#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOptGroupElement.h"

static JSBool
HTMLOptGroupElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OPT_GROUP_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_OPT_GROUP_ELEMENT_LABEL:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLOptGroupElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OPT_GROUP_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_OPT_GROUP_ELEMENT_LABEL:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLOptGroupElement_props[] = {
	{ "disabled",	JSP_HTML_OPT_GROUP_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "label",	JSP_HTML_OPT_GROUP_ELEMENT_LABEL,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLOptGroupElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLOptGroupElement_class = {
	"HTMLOptGroupElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLOptGroupElement_getProperty, HTMLOptGroupElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

