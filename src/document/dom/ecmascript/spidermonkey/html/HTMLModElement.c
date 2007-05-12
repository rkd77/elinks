#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLModElement.h"

static JSBool
HTMLModElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_MOD_ELEMENT_CITE:
		/* Write me! */
		break;
	case JSP_HTML_MOD_ELEMENT_DATE_TIME:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLModElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_MOD_ELEMENT_CITE:
		/* Write me! */
		break;
	case JSP_HTML_MOD_ELEMENT_DATE_TIME:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLModElement_props[] = {
	{ "cite",	JSP_HTML_MOD_ELEMENT_CITE,	JSPROP_ENUMERATE },
	{ "dateTime",	JSP_HTML_MOD_ELEMENT_DATE_TIME,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLModElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLModElement_class = {
	"HTMLModElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLModElement_getProperty, HTMLModElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

