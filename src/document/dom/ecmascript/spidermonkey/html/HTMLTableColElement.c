#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableColElement.h"
#include "dom/node.h"

static JSBool
HTMLTableColElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct COL_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTableColElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_COL_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_CH:
		string_to_jsval(ctx, vp, html->ch);
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_CH_OFF:
		string_to_jsval(ctx, vp, html->ch_off);
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_SPAN:
		int_to_jsval(ctx, vp, html->span);
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_VALIGN:
		string_to_jsval(ctx, vp, html->valign);
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_WIDTH:
		string_to_jsval(ctx, vp, html->width);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTableColElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct COL_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTableColElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TABLE_COL_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_CH:
		mem_free_set(&html->ch, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_CH_OFF:
		mem_free_set(&html->ch_off, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_SPAN:
		return JS_ValueToInt32(ctx, *vp, &html->span);
	case JSP_HTML_TABLE_COL_ELEMENT_VALIGN:
		mem_free_set(&html->valign, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TABLE_COL_ELEMENT_WIDTH:
		mem_free_set(&html->width, stracpy(jsval_to_string(ctx, vp)));
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

void
make_COL_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct COL_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLTableColElement_class, o->HTMLElement_object, NULL);
	}
}

