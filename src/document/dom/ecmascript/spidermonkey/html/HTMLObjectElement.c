#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLObjectElement.h"

static JSBool
HTMLObjectElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OBJECT_ELEMENT_FORM:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_ARCHIVE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE_BASE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_DATA:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_DECLARE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_HSPACE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_STANDBY:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_USE_MAP:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_VSPACE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_WIDTH:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_CONTENT_DOCUMENT:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLObjectElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_OBJECT_ELEMENT_CODE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_ARCHIVE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE_BASE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_CODE_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_DATA:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_DECLARE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_HSPACE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_STANDBY:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_TAB_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_TYPE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_USE_MAP:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_VSPACE:
		/* Write me! */
		break;
	case JSP_HTML_OBJECT_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLObjectElement_props[] = {
	{ "form",		JSP_HTML_OBJECT_ELEMENT_FORM,			JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "code",		JSP_HTML_OBJECT_ELEMENT_CODE,			JSPROP_ENUMERATE },
	{ "align",		JSP_HTML_OBJECT_ELEMENT_ALIGN,			JSPROP_ENUMERATE },
	{ "archive",		JSP_HTML_OBJECT_ELEMENT_ARCHIVE,		JSPROP_ENUMERATE },
	{ "border",		JSP_HTML_OBJECT_ELEMENT_BORDER,			JSPROP_ENUMERATE },
	{ "codeBase",		JSP_HTML_OBJECT_ELEMENT_CODE_BASE,		JSPROP_ENUMERATE },
	{ "codeType",		JSP_HTML_OBJECT_ELEMENT_CODE_TYPE,		JSPROP_ENUMERATE },
	{ "data",		JSP_HTML_OBJECT_ELEMENT_DATA,			JSPROP_ENUMERATE },
	{ "declare",		JSP_HTML_OBJECT_ELEMENT_DECLARE,		JSPROP_ENUMERATE },
	{ "height",		JSP_HTML_OBJECT_ELEMENT_HEIGHT,			JSPROP_ENUMERATE },
	{ "hspace",		JSP_HTML_OBJECT_ELEMENT_HSPACE,			JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_OBJECT_ELEMENT_NAME,			JSPROP_ENUMERATE },
	{ "standby",		JSP_HTML_OBJECT_ELEMENT_STANDBY,		JSPROP_ENUMERATE },
	{ "tabIndex",		JSP_HTML_OBJECT_ELEMENT_TAB_INDEX,		JSPROP_ENUMERATE },
	{ "useMap",		JSP_HTML_OBJECT_ELEMENT_USE_MAP,		JSPROP_ENUMERATE },
	{ "vspace",		JSP_HTML_OBJECT_ELEMENT_VSPACE,			JSPROP_ENUMERATE },
	{ "width",		JSP_HTML_OBJECT_ELEMENT_WIDTH,			JSPROP_ENUMERATE },
	{ "contentDocument",	JSP_HTML_OBJECT_ELEMENT_CONTENT_DOCUMENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

const JSFunctionSpec HTMLObjectElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLObjectElement_class = {
	"HTMLObjectElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLObjectElement_getProperty, HTMLObjectElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

