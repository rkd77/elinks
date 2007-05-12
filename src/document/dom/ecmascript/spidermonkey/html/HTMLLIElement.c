#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLIElement.h"

static JSBool
HTMLLIElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LI_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_LI_ELEMENT_VALUE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLLIElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_LI_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_LI_ELEMENT_VALUE:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLLIElement_props[] = {
	{ "type",	JSP_HTML_LI_ELEMENT_TYPE,	JSPROP_ENUMERATE },
	{ "value",	JSP_HTML_LI_ELEMENT_VALUE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLLIElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLLIElement_class = {
	"HTMLLIElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLLIElement_getProperty, HTMLLIElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

