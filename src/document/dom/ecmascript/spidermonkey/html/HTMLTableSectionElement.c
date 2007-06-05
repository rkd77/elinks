#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableSectionElement.h"

static JSBool
HTMLTableSectionElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_SECTION_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_CH:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_CH_OFF:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_VALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_ROWS:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableSectionElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_SECTION_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_CH:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_CH_OFF:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_VALIGN:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableSectionElement_insertRow(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTableSectionElement_deleteRow(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLTableSectionElement_props[] = {
	{ "align",	JSP_HTML_TABLE_SECTION_ELEMENT_ALIGN,	JSPROP_ENUMERATE },
	{ "ch",		JSP_HTML_TABLE_SECTION_ELEMENT_CH,	JSPROP_ENUMERATE },
	{ "chOff",	JSP_HTML_TABLE_SECTION_ELEMENT_CH_OFF,	JSPROP_ENUMERATE },
	{ "vAlign",	JSP_HTML_TABLE_SECTION_ELEMENT_VALIGN,	JSPROP_ENUMERATE },
	{ "rows",	JSP_HTML_TABLE_SECTION_ELEMENT_ROWS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ NULL }
};

const JSFunctionSpec HTMLTableSectionElement_funcs[] = {
	{ "insertRow",	HTMLTableSectionElement_insertRow,	1 },
	{ "deleteRow",	HTMLTableSectionElement_deleteRow,	1 },
	{ NULL }
};

const JSClass HTMLTableSectionElement_class = {
	"HTMLTableSectionElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLTableSectionElement_getProperty, HTMLTableSectionElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};
