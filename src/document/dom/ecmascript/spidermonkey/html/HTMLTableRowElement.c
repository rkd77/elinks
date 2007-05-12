#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableRowElement.h"

static JSBool
HTMLTableRowElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_ROW_ELEMENT_ROW_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_SECTION_ROW_INDEX:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CELLS:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_BGCOLOR:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CH:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CH_OFF:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_VALIGN:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableRowElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_ROW_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_BGCOLOR:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CH:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CH_OFF:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_VALIGN:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableRowElement_insertCell(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTableRowElement_deleteCell(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLTableRowElement_props[] = {
	{ "rowIndex",		JSP_HTML_TABLE_ROW_ELEMENT_ROW_INDEX,		JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "sectionRowIndex",	JSP_HTML_TABLE_ROW_ELEMENT_SECTION_ROW_INDEX,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "cells",		JSP_HTML_TABLE_ROW_ELEMENT_CELLS,		JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "align",		JSP_HTML_TABLE_ROW_ELEMENT_ALIGN,		JSPROP_ENUMERATE },
	{ "bgColor",		JSP_HTML_TABLE_ROW_ELEMENT_BGCOLOR,		JSPROP_ENUMERATE },
	{ "ch",			JSP_HTML_TABLE_ROW_ELEMENT_CH,			JSPROP_ENUMERATE },
	{ "chOff",		JSP_HTML_TABLE_ROW_ELEMENT_CH_OFF,		JSPROP_ENUMERATE },
	{ "vAlign",		JSP_HTML_TABLE_ROW_ELEMENT_VALIGN,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLTableRowElement_funcs[] = {
	{ "insertCell",	HTMLTableRowElement_insertCell,	1 },
	{ "deleteCell",	HTMLTableRowElement_deleteCell,	1 },
	{ NULL }
};

const JSClass HTMLTableRowElement_class = {
	"HTMLTableRowElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLTableRowElement_getProperty, HTMLTableRowElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

