#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Element.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"

JSBool
HTMLElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ELEMENT_ID:
		/* Write me! */
		break;
	case JSP_HTML_ELEMENT_TITLE:
		/* Write me! */
		break;
	case JSP_HTML_ELEMENT_LANG:
		/* Write me! */
		break;
	case JSP_HTML_ELEMENT_DIR:
		/* Write me! */
		break;
	case JSP_HTML_ELEMENT_CLASS_NAME:
		/* Write me! */
		break;
	default:
		return Element_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

JSBool
HTMLElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ELEMENT_ID:
		/* Write me! */
		break;
	case JSP_HTML_ELEMENT_TITLE:
		/* Write me! */
		break;
	case JSP_HTML_ELEMENT_LANG:
		/* Write me! */
		break;
	case JSP_HTML_ELEMENT_DIR:
		/* Write me! */
		break;
	case JSP_HTML_ELEMENT_CLASS_NAME:
		/* Write me! */
		break;
	default:
		return Node_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLElement_props[] = {
	{ "id",		JSP_HTML_ELEMENT_ID,		JSPROP_ENUMERATE },
	{ "title",	JSP_HTML_ELEMENT_TITLE,		JSPROP_ENUMERATE },
	{ "lang",	JSP_HTML_ELEMENT_LANG,		JSPROP_ENUMERATE },
	{ "dir",	JSP_HTML_ELEMENT_DIR,		JSPROP_ENUMERATE },
	{ "className",	JSP_HTML_ELEMENT_CLASS_NAME,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLElement_class = {
	"HTMLElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLElement_getProperty, HTMLElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

