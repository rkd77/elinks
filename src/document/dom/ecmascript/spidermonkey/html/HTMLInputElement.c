#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLInputElement.h"

static JSBool
HTMLInputElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_INPUT_ELEMENT_DEFAULT_VALUE:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_DEFAULT_CHECKED:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_FORM:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_ACCEPT:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_ALT:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_CHECKED:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_MAX_LENGTH:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_READ_ONLY:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_SIZE:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_SRC:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_USE_MAP:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_VALUE:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLInputElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_INPUT_ELEMENT_DEFAULT_VALUE:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_DEFAULT_CHECKED:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_ACCEPT:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_ACCESS_KEY:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_ALT:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_CHECKED:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_DISABLED:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_MAX_LENGTH:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_READ_ONLY:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_SIZE:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_SRC:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_USE_MAP:
		/* Write me! */
		break;
	case JSP_HTML_INPUT_ELEMENT_VALUE:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLInputElement_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLInputElement_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLInputElement_select(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLInputElement_click(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLInputElement_props[] = {
	{ "defaultValue",	JSP_HTML_INPUT_ELEMENT_DEFAULT_VALUE,	JSPROP_ENUMERATE },
	{ "defaultChecked",	JSP_HTML_INPUT_ELEMENT_DEFAULT_CHECKED,	JSPROP_ENUMERATE },
	{ "form",		JSP_HTML_INPUT_ELEMENT_FORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "accept",		JSP_HTML_INPUT_ELEMENT_ACCEPT,		JSPROP_ENUMERATE },
	{ "accessKey",		JSP_HTML_INPUT_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "align",		JSP_HTML_INPUT_ELEMENT_ALIGN,		JSPROP_ENUMERATE },
	{ "alt",		JSP_HTML_INPUT_ELEMENT_ALT,		JSPROP_ENUMERATE },
	{ "checked",		JSP_HTML_INPUT_ELEMENT_CHECKED,		JSPROP_ENUMERATE },
	{ "disabled",		JSP_HTML_INPUT_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "maxLength",		JSP_HTML_INPUT_ELEMENT_MAX_LENGTH,	JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_INPUT_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "readOnly",		JSP_HTML_INPUT_ELEMENT_READ_ONLY,	JSPROP_ENUMERATE },
	{ "size",		JSP_HTML_INPUT_ELEMENT_SIZE,		JSPROP_ENUMERATE },
	{ "src",		JSP_HTML_INPUT_ELEMENT_SRC,		JSPROP_ENUMERATE },
	{ "tabIndex",		JSP_HTML_INPUT_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ "type",		JSP_HTML_INPUT_ELEMENT_TYPE,		JSPROP_ENUMERATE },
	{ "useMap",		JSP_HTML_INPUT_ELEMENT_USE_MAP,		JSPROP_ENUMERATE },
	{ "value",		JSP_HTML_INPUT_ELEMENT_VALUE,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLInputElement_funcs[] = {
	{ "blur",	HTMLInputElement_blur,		0 },
	{ "focus",	HTMLInputElement_focus,		0 },
	{ "select",	HTMLInputElement_select,	0 },
	{ "click",	HTMLInputElement_click,		0 },
	{ NULL }
};

const JSClass HTMLInputElement_class = {
	"HTMLInputElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLInputElement_getProperty, HTMLInputElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

