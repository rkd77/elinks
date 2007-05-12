#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTextAreaElement.h"

static JSBool
HTMLTextAreaElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TEXT_AREA_ELEMENT_DEFAULT_VALUE:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_FORM:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_COLS:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_READ_ONLY:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ROWS:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_VALUE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTextAreaElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TEXT_AREA_ELEMENT_DEFAULT_VALUE:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_COLS:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_READ_ONLY:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ROWS:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_VALUE:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTextAreaElement_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTextAreaElement_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTextAreaElement_select(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLTextAreaElement_props[] = {
	{ "defaultValue",	JSP_HTML_TEXT_AREA_ELEMENT_DEFAULT_VALUE,	JSPROP_ENUMERATE },
	{ "form",	JSP_HTML_TEXT_AREA_ELEMENT_FORM,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "accessKey",	JSP_HTML_TEXT_AREA_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "cols",	JSP_HTML_TEXT_AREA_ELEMENT_COLS,	JSPROP_ENUMERATE },
	{ "disabled",	JSP_HTML_TEXT_AREA_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "name",	JSP_HTML_TEXT_AREA_ELEMENT_NAME,	JSPROP_ENUMERATE },
	{ "readOnly",	JSP_HTML_TEXT_AREA_ELEMENT_READ_ONLY,	JSPROP_ENUMERATE },
	{ "rows",	JSP_HTML_TEXT_AREA_ELEMENT_ROWS,	JSPROP_ENUMERATE },
	{ "tabIndex",	JSP_HTML_TEXT_AREA_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_TEXT_AREA_ELEMENT_TYPE,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "value",	JSP_HTML_TEXT_AREA_ELEMENT_DEFAULT_VALUE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLTextAreaElement_funcs[] = {
	{ "blur",	HTMLTextAreaElement_blur,	0 },
	{ "focus",	HTMLTextAreaElement_focus,	0 },
	{ "select",	HTMLTextAreaElement_select,	0 },
	{ NULL }
};

const JSClass HTMLTextAreaElement_class = {
	"HTMLTextAreaElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLTextAreaElement_getProperty, HTMLTextAreaElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

