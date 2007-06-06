#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableRowElement.h"
#include "dom/node.h"

static JSBool
HTMLTableRowElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct TR_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTableRowElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_ROW_ELEMENT_ROW_INDEX:
		int_to_jsval(ctx, vp, html->row_index);
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_SECTION_ROW_INDEX:
		int_to_jsval(ctx, vp, html->section_row_index);
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CELLS:
		string_to_jsval(ctx, vp, html->cells);
		/* Write me! */
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_BGCOLOR:
		string_to_jsval(ctx, vp, html->bgcolor);
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CH:
		string_to_jsval(ctx, vp, html->ch);
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CH_OFF:
		string_to_jsval(ctx, vp, html->ch_off);
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_VALIGN:
		string_to_jsval(ctx, vp, html->valign);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableRowElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct TR_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTableRowElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_ROW_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_BGCOLOR:
		mem_free_set(&html->bgcolor, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CH:
		mem_free_set(&html->ch, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_CH_OFF:
		mem_free_set(&html->ch_off, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_ROW_ELEMENT_VALIGN:
		mem_free_set(&html->valign, stracpy(jsval_to_string(ctx, vp)));
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
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_TR_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct TR_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLTableRowElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_TR_object(void *data)
{
	struct TR_struct *d = data;

	/* d->cells ? */
	mem_free_if(d->align);
	mem_free_if(d->bgcolor);
	mem_free_if(d->ch);
	mem_free_if(d->ch_off);
	mem_free_if(d->valign);
}
