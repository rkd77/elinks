#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLSelectElement.h"

static JSBool
HTMLSelectElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_SELECT_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_SELECTED_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_VALUE:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_LENGTH:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_FORM:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_OPTIONS:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_MULTIPLE:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_SIZE:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_SELECT_ELEMENT_SELECTED_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_VALUE:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_LENGTH:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_MULTIPLE:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_SIZE:
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_add(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_remove(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLSelectElement_props[] = {
	{ "type",		JSP_HTML_SELECT_ELEMENT_TYPE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "selectedIndex",	JSP_HTML_SELECT_ELEMENT_SELECTED_INDEX,	JSPROP_ENUMERATE },
	{ "value",		JSP_HTML_SELECT_ELEMENT_VALUE,		JSPROP_ENUMERATE },
	{ "length",		JSP_HTML_SELECT_ELEMENT_LENGTH,		JSPROP_ENUMERATE },
	{ "form",		JSP_HTML_SELECT_ELEMENT_FORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "options",		JSP_HTML_SELECT_ELEMENT_OPTIONS,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "disabled",		JSP_HTML_SELECT_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "multiple",		JSP_HTML_SELECT_ELEMENT_MULTIPLE,	JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_SELECT_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "size",		JSP_HTML_SELECT_ELEMENT_SIZE,		JSPROP_ENUMERATE },
	{ "tabIndex",		JSP_HTML_SELECT_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLSelectElement_funcs[] = {
	{ "add",	HTMLSelectElement_add,		1 },
	{ "remove",	HTMLSelectElement_remove,	2 },
	{ "blur",	HTMLSelectElement_blur,		0 },
	{ "focus",	HTMLSelectElement_focus,	0 },
	{ NULL }
};

const JSClass HTMLSelectElement_class = {
	"HTMLSelectElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLSelectElement_getProperty, HTMLSelectElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

