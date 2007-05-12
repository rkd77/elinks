#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableElement.h"

static JSBool
HTMLTableElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_ELEMENT_CAPTION:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_THEAD:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_TFOOT:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_ROWS:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_TBODIES:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_BGCOLOR:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_CELL_PADDING:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_CELL_SPACING:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_FRAME:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_RULES:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_SUMMARY:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_ELEMENT_CAPTION:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_THEAD:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_TFOOT:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_ALIGN:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_BGCOLOR:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_BORDER:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_CELL_PADDING:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_CELL_SPACING:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_FRAME:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_RULES:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_SUMMARY:
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_WIDTH:
		/* Write me! */
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableElement_createTHead(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTableElement_deleteTHead(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTableElement_createTFoot(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTableElement_deleteTFoot(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTableElement_createCaption(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTableElement_deleteCaption(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTableElement_insertRow(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTableElement_deleteRow(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLTableElement_props[] = {
	{ "caption",	JSP_HTML_TABLE_ELEMENT_CAPTION,		JSPROP_ENUMERATE },
	{ "tHead",	JSP_HTML_TABLE_ELEMENT_THEAD,		JSPROP_ENUMERATE },
	{ "tFoot",	JSP_HTML_TABLE_ELEMENT_TFOOT,		JSPROP_ENUMERATE },
	{ "rows",	JSP_HTML_TABLE_ELEMENT_ROWS,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "tBodies",	JSP_HTML_TABLE_ELEMENT_TBODIES,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "align",	JSP_HTML_TABLE_ELEMENT_ALIGN,		JSPROP_ENUMERATE },
	{ "bgColor",	JSP_HTML_TABLE_ELEMENT_BGCOLOR,		JSPROP_ENUMERATE },
	{ "border",	JSP_HTML_TABLE_ELEMENT_BORDER,		JSPROP_ENUMERATE },
	{ "cellPadding",JSP_HTML_TABLE_ELEMENT_CELL_PADDING,	JSPROP_ENUMERATE },
	{ "cellSpacing",JSP_HTML_TABLE_ELEMENT_CELL_SPACING,	JSPROP_ENUMERATE },
	{ "frame",	JSP_HTML_TABLE_ELEMENT_FRAME,		JSPROP_ENUMERATE },
	{ "rules",	JSP_HTML_TABLE_ELEMENT_RULES,		JSPROP_ENUMERATE },
	{ "summary",	JSP_HTML_TABLE_ELEMENT_SUMMARY,		JSPROP_ENUMERATE },
	{ "width",	JSP_HTML_TABLE_ELEMENT_WIDTH,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLTableElement_funcs[] = {
	{ "createTHead",	HTMLTableElement_createTHead,	0 },
	{ "deleteTHead",	HTMLTableElement_deleteTHead,	0 },
	{ "createTFoot",	HTMLTableElement_createTFoot,	0 },
	{ "deleteTFoot",	HTMLTableElement_deleteTFoot,	0 },
	{ "createCaption",	HTMLTableElement_createCaption,	0 },
	{ "deleteCaption",	HTMLTableElement_deleteCaption,	0 },
	{ "insertRow",		HTMLTableElement_insertRow,	1 },
	{ "deleteRow",		HTMLTableElement_deleteRow,	1 },
	{ NULL }
};

const JSClass HTMLTableElement_class = {
	"HTMLTableElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLTableElement_getProperty, HTMLTableElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

