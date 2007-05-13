#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableColElement.h"

static JSBool
HTMLTableColElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_COL_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_CH:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_CH_OFF:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_SPAN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_VALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableColElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_COL_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_CH:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_CH_OFF:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_SPAN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_VALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLTableColElement_props[] = {
	{ "align",	JSP_HTML_TABLE_COL_ELEMENT_ALIGN,	JSPROP_ENUMERATE },
	{ "ch",		JSP_HTML_TABLE_COL_ELEMENT_CH,		JSPROP_ENUMERATE },
	{ "chOff",	JSP_HTML_TABLE_COL_ELEMENT_CH_OFF,	JSPROP_ENUMERATE },
	{ "span",	JSP_HTML_TABLE_COL_ELEMENT_SPAN,	JSPROP_ENUMERATE },
	{ "vAlign",	JSP_HTML_TABLE_COL_ELEMENT_VALIGN,	JSPROP_ENUMERATE },
	{ "width",	JSP_HTML_TABLE_COL_ELEMENT_WIDTH,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLTableColElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLTableColElement_class = {
	"HTMLTableColElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLTableColElement_getProperty, HTMLTableColElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

