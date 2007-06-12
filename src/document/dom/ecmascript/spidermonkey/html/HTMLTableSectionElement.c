#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLCollection.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableSectionElement.h"
#include "dom/node.h"
#include "dom/sgml/html/html.h"

static JSBool
HTMLTableSectionElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct THEAD_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTableSectionElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_SECTION_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_CH:
		string_to_jsval(ctx, vp, html->ch);
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_CH_OFF:
		string_to_jsval(ctx, vp, html->chOff);
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_VALIGN:
		string_to_jsval(ctx, vp, html->vAlign);
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_ROWS:
		if (!html->rows)
			undef_to_jsval(ctx, vp);
		else {
			if (html->rows->ctx == ctx)
				object_to_jsval(ctx, vp, html->rows->ecmascript_obj);
			else {
				JSObject *new_obj;

				html->rows->ctx = ctx;
				new_obj = JS_NewObject(ctx,
				 (JSClass *)&HTMLCollection_class, NULL, NULL);
				if (new_obj) {
					JS_SetPrivate(ctx, new_obj, html->rows);
				}
				html->rows->ecmascript_obj = new_obj;
				object_to_jsval(ctx, vp, new_obj);
			}
		}
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableSectionElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct THEAD_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTableSectionElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_SECTION_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_CH:
		mem_free_set(&html->ch, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_CH_OFF:
		mem_free_set(&html->chOff, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_SECTION_ELEMENT_VALIGN:
		mem_free_set(&html->vAlign, stracpy(jsval_to_string(ctx, vp)));
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

void
make_THEAD_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct THEAD_struct));
	if (node->data.element.html_data) {
		struct dom_node *table;

		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLTableSectionElement_class, o->HTMLElement_object, NULL);
		table = find_parent_table(node);
		if (table) {
			struct TABLE_struct *html = table->data.element.html_data;

			if (!html)
				return;
			switch (node->data.element.type) {
			case HTML_ELEMENT_THEAD:
				html->thead = node;
				break;
			case HTML_ELEMENT_TFOOT:
				html->tfoot = node;
				break;
			case HTML_ELEMENT_TBODY:
				register_tbody(table, node);
				break;
			default:
					break;
			}
		}
	}
}

void
done_THEAD_object(struct dom_node *node)
{
	struct THEAD_struct *d = node->data.element.html_data;

	if (d->rows) {
		if (d->rows->ecmascript_obj)
			JS_SetPrivate(d->rows->ctx, d->rows->ecmascript_obj, NULL);
		mem_free(d->rows);
	}
	if (node->data.element.type == HTML_ELEMENT_TBODY)
		unregister_tbody(node);
	mem_free_if(d->align);
	mem_free_if(d->ch);
	mem_free_if(d->chOff);
	mem_free_if(d->vAlign);
}

void
register_section_row(struct dom_node *node)
{
	/* Write me! */
}

void
unregister_section_row(struct dom_node *node)
{
	/* Write me! */
}
