#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLCollection.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableSectionElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableElement.h"
#include "dom/node.h"
#include "dom/sgml/html/html.h"

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
		if (html->caption)
			object_to_jsval(ctx, vp, html->caption->ecmascript_obj);
		else
			undef_to_jsval(ctx, vp);
		break;
	case JSP_HTML_TABLE_ELEMENT_THEAD:
		if (html->thead)
			object_to_jsval(ctx, vp, html->thead->ecmascript_obj);
		else
			undef_to_jsval(ctx, vp);
		break;
	case JSP_HTML_TABLE_ELEMENT_TFOOT:
		if (html->tfoot)
			object_to_jsval(ctx, vp, html->tfoot->ecmascript_obj);
		else
			undef_to_jsval(ctx, vp);
		break;
	case JSP_HTML_TABLE_ELEMENT_ROWS:
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
				if (new_obj)
					JS_SetPrivate(ctx, new_obj, html->rows);
				html->rows->ecmascript_obj = new_obj;
				object_to_jsval(ctx, vp, new_obj);
			}
		}
		break;
	case JSP_HTML_TABLE_ELEMENT_TBODIES:
		if (!html->tbodies)
			undef_to_jsval(ctx, vp);
		else {
			if (html->tbodies->ctx == ctx)
				object_to_jsval(ctx, vp, html->tbodies->ecmascript_obj);
			else {
				JSObject *new_obj;

				html->tbodies->ctx = ctx;
				new_obj = JS_NewObject(ctx,
				 (JSClass *)&HTMLCollection_class, NULL, NULL);

				if (new_obj)
					JS_SetPrivate(ctx, new_obj, html->tbodies);
				html->tbodies->ecmascript_obj = new_obj;
				object_to_jsval(ctx, vp, new_obj);
			}
		}
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
	JSObject *value;

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
		value = JSVAL_TO_OBJECT(*vp);
		if (value) {
			struct dom_node *caption = JS_GetPrivate(ctx, value);

			html->caption = caption;
		}
		break;
	case JSP_HTML_TABLE_ELEMENT_THEAD:
		value = JSVAL_TO_OBJECT(*vp);
		if (value) {
			struct dom_node *thead = JS_GetPrivate(ctx, value);

			html->thead = thead;
		}
		break;
	case JSP_HTML_TABLE_ELEMENT_TFOOT:
		value = JSVAL_TO_OBJECT(*vp);
		if (value) {
			struct dom_node *tfoot = JS_GetPrivate(ctx, value);

			html->tfoot = tfoot;
		}
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
	struct TABLE_struct *t = mem_calloc(1, sizeof(struct TABLE_struct));
	struct dom_node *tbody;

	if (!t)
		return;

	node->data.element.html_data = t;
	node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLTableElement_class, o->HTMLElement_object, NULL);
	/* Alloc node for implicit tbody element. */
	tbody = mem_calloc(1, sizeof(*tbody));
	if (tbody) {
		add_to_dom_node_list(&t->tbodies, tbody, -1);
		if (!t->tbodies)
			mem_free(tbody);
		tbody->parent = node;
	}
}

void
done_TABLE_object(struct dom_node *node)
{
	struct TABLE_struct *d = node->data.element.html_data;

	if (d->rows) {
		if (d->rows->ecmascript_obj)
			JS_SetPrivate(d->rows->ctx, d->rows->ecmascript_obj, NULL);
		mem_free(d->rows);
	}
	if (d->tbodies) {
		if (d->tbodies->ecmascript_obj)
			JS_SetPrivate(d->tbodies->ctx, d->tbodies->ecmascript_obj, NULL);
		mem_free_if(d->tbodies->entries[0]);
		mem_free(d->tbodies);
	}
	mem_free_if(d->align);
	mem_free_if(d->bgcolor);
	mem_free_if(d->border);
	mem_free_if(d->cell_padding);
	mem_free_if(d->cell_spacing);
	mem_free_if(d->frame);
	mem_free_if(d->rules);
	mem_free_if(d->summary);
	mem_free_if(d->width);
}

struct dom_node *
find_parent_table(struct dom_node *node)
{
	while (1) {
		node = node->parent;
		if (!node)
			break;
		if (node->type == DOM_NODE_ELEMENT && node->data.element.type == HTML_ELEMENT_TABLE)
			break;
	}
	return node;
}

void
register_row(struct dom_node *node)
{
	struct dom_node *table;
	struct dom_node *cur = node;
	int found = 0;

	while (!found) {
		cur = cur->parent;
		if (!cur)
			return;
		if (cur->type == DOM_NODE_ELEMENT) {
			switch (cur->data.element.type) {
			case HTML_ELEMENT_TBODY:
			case HTML_ELEMENT_TFOOT:
			case HTML_ELEMENT_THEAD:
				{
					struct THEAD_struct *d = cur->data.element.html_data;

					add_to_dom_node_list(&d->rows, node, -1);
					found = 1;
				}
				break;
			case HTML_ELEMENT_TABLE:
				{
					struct dom_node *tbody;
					struct TABLE_struct *t;
					struct THEAD_struct *tb;

					table = cur;
					t = table->data.element.html_data;
					tbody = t->tbodies->entries[0];
					tb = tbody->data.element.html_data;

					add_to_dom_node_list(&tb->rows, node, -1);
					goto fin;
				}
			}
		}
	}
	table = find_parent_table(cur);
fin:
	if (table) {
		struct TABLE_struct *d = table->data.element.html_data;

		add_to_dom_node_list(&d->rows, node, -1);
	}
}

void
unregister_row(struct dom_node *node)
{
	struct dom_node *table;
	struct dom_node *cur = node;
	int found = 0;

	while (!found) {
		cur = cur->parent;
		if (!cur)
			return;
		if (cur->type == DOM_NODE_ELEMENT) {
			switch (cur->data.element.type) {
			case HTML_ELEMENT_TBODY:
			case HTML_ELEMENT_TFOOT:
			case HTML_ELEMENT_THEAD:
				{
					struct THEAD_struct *d = cur->data.element.html_data;

					del_from_dom_node_list(d->rows, node);
					found = 1;
				}
				break;
			case HTML_ELEMENT_TABLE:
				{
					struct dom_node *tbody;
					struct TABLE_struct *t;
					struct THEAD_struct *tb;

					table = cur;
					t = table->data.element.html_data;
					tbody = t->tbodies->entries[0];
					tb = tbody->data.element.html_data;

					del_from_dom_node_list(tb->rows, node);
					goto fin;
				}
			}
		}
	}
	table = find_parent_table(cur);
fin:
	if (table) {
		struct TABLE_struct *d = table->data.element.html_data;

		del_from_dom_node_list(d->rows, node);
	}
}

void
register_tbody(struct dom_node *table, struct dom_node *node)
{
		struct TABLE_struct *d = table->data.element.html_data;

		add_to_dom_node_list(&d->rows, node, -1);
}

void
unregister_tbody(struct dom_node *node)
{
	struct dom_node *table = find_parent_table(node);

	if (table) {
		struct TABLE_struct *d = table->data.element.html_data;

		del_from_dom_node_list(d->tbodies, node);
	}
}
