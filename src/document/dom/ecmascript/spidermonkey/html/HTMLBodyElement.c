#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBodyElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"

static JSBool
HTMLBodyElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BODY_ELEMENT_ALINK:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_BACKGROUND:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_BGCOLOR:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_LINK:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_TEXT:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_VLINK:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLBodyElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_BODY_ELEMENT_ALINK:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_BACKGROUND:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_BGCOLOR:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_LINK:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_TEXT:
		/* Write me! */
		break;
	case JSP_HTML_BODY_ELEMENT_VLINK:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLBodyElement_props[] = {
	{ "aLink",	JSP_HTML_BODY_ELEMENT_ALINK,		JSPROP_ENUMERATE },
	{ "background",	JSP_HTML_BODY_ELEMENT_BACKGROUND,	JSPROP_ENUMERATE },
	{ "bgColor",	JSP_HTML_BODY_ELEMENT_BGCOLOR,		JSPROP_ENUMERATE },
	{ "link",	JSP_HTML_BODY_ELEMENT_LINK,		JSPROP_ENUMERATE },
	{ "text",	JSP_HTML_BODY_ELEMENT_TEXT,		JSPROP_ENUMERATE },
	{ "vLink",	JSP_HTML_BODY_ELEMENT_VLINK,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLBodyElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLBodyElement_class = {
	"HTMLBodyElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLBodyElement_getProperty, HTMLBodyElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

