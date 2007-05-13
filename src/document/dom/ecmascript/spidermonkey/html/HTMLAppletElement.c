#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLAppletElement.h"

static JSBool
HTMLAppletElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_APPLET_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_ALT:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_ARCHIVE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_CODE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_CODE_BASE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_HSPACE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_OBJECT:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_VSPACE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLAppletElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_APPLET_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_ALT:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_ARCHIVE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_CODE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_CODE_BASE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_HSPACE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_NAME:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_OBJECT:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_VSPACE:
		/* Write me! */
		break;
	case JSP_HTML_APPLET_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLAppletElement_props[] = {
	{ "align",	JSP_HTML_APPLET_ELEMENT_ALIGN,		JSPROP_ENUMERATE },
	{ "alt",	JSP_HTML_APPLET_ELEMENT_ALT,		JSPROP_ENUMERATE },
	{ "archive",	JSP_HTML_APPLET_ELEMENT_ARCHIVE,	JSPROP_ENUMERATE },
	{ "code",	JSP_HTML_APPLET_ELEMENT_CODE,		JSPROP_ENUMERATE },
	{ "codeBase",	JSP_HTML_APPLET_ELEMENT_CODE_BASE,	JSPROP_ENUMERATE },
	{ "height",	JSP_HTML_APPLET_ELEMENT_HEIGHT,		JSPROP_ENUMERATE },
	{ "hspace",	JSP_HTML_APPLET_ELEMENT_HSPACE,		JSPROP_ENUMERATE },
	{ "name",	JSP_HTML_APPLET_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "object",	JSP_HTML_APPLET_ELEMENT_OBJECT,		JSPROP_ENUMERATE },
	{ "vspace",	JSP_HTML_APPLET_ELEMENT_VSPACE,		JSPROP_ENUMERATE },
	{ "width",	JSP_HTML_APPLET_ELEMENT_WIDTH,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLAppletElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLAppletElement_class = {
	"HTMLAppletElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLAppletElement_getProperty, HTMLAppletElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

