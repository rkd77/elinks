#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLCollection.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableRowElement.h"
#include "dom/node.h"
#include "dom/sgml/html/html.h"

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
		if (!html->cells)
			undef_to_jsval(ctx, vp);
		else {
			if (html->cells->ctx == ctx)
				object_to_jsval(ctx, vp, html->cells->ecmascript_obj);
			else {
				JSObject *new_obj;

				html->cells->ctx = ctx;
				new_obj = JS_NewObject(ctx,
				 (JSClass *)&HTMLCollection_class, NULL, NULL);
				if (new_obj)
					JS_SetPrivate(ctx, new_obj, html->cells);
				html->cells->ecmascript_obj = new_obj;
				object_to_jsval(ctx, vp, new_obj);
			}
		}
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

static struct dom_node *
find_parent_row(struct dom_node *node)
{
	struct dom_node *row = node;

	while (1) {
		row = row->parent;
		if (!row)
			break;
		if (row->type == DOM_NODE_ELEMENT && row->data.element.type == HTML_ELEMENT_TR)
			break;
	}
	return row;
}

void
register_cell(struct dom_node *node)
{
	struct dom_node *row = find_parent_row(node);

	if (row) {
		struct TR_struct *html = row->data.element.html_data;

		if (html) {
			add_to_dom_node_list(&html->cells, node, -1);
		}
	}
}

void
unregister_cell(struct dom_node *node)
{
	struct dom_node *row = find_parent_row(node);

	if (row) {
		struct TR_struct *html = row->data.element.html_data;

		if (html) {
			del_from_dom_node_list(html->cells, node);
		}
	}
}

void
make_TR_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct TR_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLTableRowElement_class, o->HTMLElement_object, NULL);
		register_row(node);
	}
}

void
done_TR_object(struct dom_node *node)
{
	struct TR_struct *d = node->data.element.html_data;

	unregister_row(node);
	if (d->cells) {
		JS_SetPrivate(d->cells->ctx, d->cells->ecmascript_obj, NULL);
		mem_free(d->cells);
	}
	mem_free_if(d->align);
	mem_free_if(d->bgcolor);
	mem_free_if(d->ch);
	mem_free_if(d->ch_off);
	mem_free_if(d->valign);
}
