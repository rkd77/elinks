#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableCellElement.h"

static JSBool
HTMLTableCellElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_CELL_ELEMENT_CELL_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_ABBR:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_AXIS:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_BGCOLOR:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_CH:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_CH_OFF:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_COL_SPAN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_HEADERS:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_NO_WRAP:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_ROW_SPAN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_SCOPE:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_VALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableCellElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_CELL_ELEMENT_ABBR:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_AXIS:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_BGCOLOR:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_CH:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_CH_OFF:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_COL_SPAN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_HEADERS:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_HEIGHT:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_NO_WRAP:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_ROW_SPAN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_SCOPE:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_VALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_CELL_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLTableCellElement_props[] = {
	{ "cellIndex",	JSP_HTML_TABLE_CELL_ELEMENT_CELL_INDEX,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "abbr",	JSP_HTML_TABLE_CELL_ELEMENT_ABBR,	JSPROP_ENUMERATE },
	{ "align",	JSP_HTML_TABLE_CELL_ELEMENT_ALIGN,	JSPROP_ENUMERATE },
	{ "axis",	JSP_HTML_TABLE_CELL_ELEMENT_AXIS,	JSPROP_ENUMERATE },
	{ "bgColor",	JSP_HTML_TABLE_CELL_ELEMENT_BGCOLOR,	JSPROP_ENUMERATE },
	{ "ch",		JSP_HTML_TABLE_CELL_ELEMENT_CH,		JSPROP_ENUMERATE },
	{ "chOff",	JSP_HTML_TABLE_CELL_ELEMENT_CH_OFF,	JSPROP_ENUMERATE },
	{ "colSpan",	JSP_HTML_TABLE_CELL_ELEMENT_COL_SPAN,	JSPROP_ENUMERATE },
	{ "headers",	JSP_HTML_TABLE_CELL_ELEMENT_HEADERS,	JSPROP_ENUMERATE },
	{ "height",	JSP_HTML_TABLE_CELL_ELEMENT_HEIGHT,	JSPROP_ENUMERATE },
	{ "noWrap",	JSP_HTML_TABLE_CELL_ELEMENT_NO_WRAP,	JSPROP_ENUMERATE },
	{ "rowSpan",	JSP_HTML_TABLE_CELL_ELEMENT_ROW_SPAN,	JSPROP_ENUMERATE },
	{ "scope",	JSP_HTML_TABLE_CELL_ELEMENT_SCOPE,	JSPROP_ENUMERATE },
	{ "vAlign",	JSP_HTML_TABLE_CELL_ELEMENT_VALIGN,	JSPROP_ENUMERATE },
	{ "width",	JSP_HTML_TABLE_CELL_ELEMENT_WIDTH,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLTableCellElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLTableCellElement_class = {
	"HTMLTableCellElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLTableCellElement_getProperty, HTMLTableCellElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

