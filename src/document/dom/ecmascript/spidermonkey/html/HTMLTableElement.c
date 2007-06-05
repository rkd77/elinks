#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableElement.h"
#include "dom/node.h"

static JSBool
HTMLTableElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct TABLE_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTableElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_ELEMENT_CAPTION:
		string_to_jsval(ctx, vp, html->caption);
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_THEAD:
		string_to_jsval(ctx, vp, html->thead);
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_TFOOT:
		string_to_jsval(ctx, vp, html->tfoot);
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_ROWS:
		string_to_jsval(ctx, vp, html->rows);
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_TBODIES:
		string_to_jsval(ctx, vp, html->tbodies);
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_TABLE_ELEMENT_BGCOLOR:
		string_to_jsval(ctx, vp, html->bgcolor);
		break;
	case JSP_HTML_TABLE_ELEMENT_BORDER:
		string_to_jsval(ctx, vp, html->border);
		break;
	case JSP_HTML_TABLE_ELEMENT_CELL_PADDING:
		string_to_jsval(ctx, vp, html->cell_padding);
		break;
	case JSP_HTML_TABLE_ELEMENT_CELL_SPACING:
		string_to_jsval(ctx, vp, html->cell_spacing);
		break;
	case JSP_HTML_TABLE_ELEMENT_FRAME:
		string_to_jsval(ctx, vp, html->frame);
		break;
	case JSP_HTML_TABLE_ELEMENT_RULES:
		string_to_jsval(ctx, vp, html->rules);
		break;
	case JSP_HTML_TABLE_ELEMENT_SUMMARY:
		string_to_jsval(ctx, vp, html->summary);
		break;
	case JSP_HTML_TABLE_ELEMENT_WIDTH:
		string_to_jsval(ctx, vp, html->width);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct TABLE_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTableElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_ELEMENT_CAPTION:
		mem_free_set(&html->caption, stracpy(jsval_to_string(ctx, vp)));
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_THEAD:
		mem_free_set(&html->thead, stracpy(jsval_to_string(ctx, vp)));
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_TFOOT:
		mem_free_set(&html->tfoot, stracpy(jsval_to_string(ctx, vp)));
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ELEMENT_BGCOLOR:
		mem_free_set(&html->bgcolor, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ELEMENT_BORDER:
		mem_free_set(&html->border, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ELEMENT_CELL_PADDING:
		mem_free_set(&html->cell_padding, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ELEMENT_CELL_SPACING:
		mem_free_set(&html->cell_spacing, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ELEMENT_FRAME:
		mem_free_set(&html->frame, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ELEMENT_RULES:
		mem_free_set(&html->rules, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ELEMENT_SUMMARY:
		mem_free_set(&html->summary, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ELEMENT_WIDTH:
		mem_free_set(&html->width, stracpy(jsval_to_string(ctx, vp)));
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
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_TABLE_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct TABLE_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLTableElement_class, o->HTMLElement_object, NULL);
	}
}

