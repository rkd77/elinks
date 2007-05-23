#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTextAreaElement.h"
#include "dom/node.h"

static JSBool
HTMLTextAreaElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct TEXTAREA_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTextAreaElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TEXT_AREA_ELEMENT_DEFAULT_VALUE:
		string_to_jsval(ctx, vp, html->default_value);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_FORM:
		string_to_jsval(ctx, vp, html->form);
		/* Write me! */
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ACCESS_KEY:
		string_to_jsval(ctx, vp, html->access_key);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_COLS:
		string_to_jsval(ctx, vp, html->cols);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_DISABLED:
		string_to_jsval(ctx, vp, html->disabled);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_READ_ONLY:
		string_to_jsval(ctx, vp, html->read_only);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ROWS:
		string_to_jsval(ctx, vp, html->rows);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_TAB_INDEX:
		string_to_jsval(ctx, vp, html->tab_index);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_VALUE:
		string_to_jsval(ctx, vp, html->value);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTextAreaElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct TEXTAREA_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLTextAreaElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_TEXT_AREA_ELEMENT_DEFAULT_VALUE:
		mem_free_set(&html->default_value, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ACCESS_KEY:
		mem_free_set(&html->access_key, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_COLS:
		mem_free_set(&html->cols, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_DISABLED:
		mem_free_set(&html->disabled, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_READ_ONLY:
		mem_free_set(&html->read_only, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ROWS:
		mem_free_set(&html->rows, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_TAB_INDEX:
		mem_free_set(&html->tab_index, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_VALUE:
		mem_free_set(&html->value, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLTextAreaElement_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTextAreaElement_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLTextAreaElement_select(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLTextAreaElement_props[] = {
	{ "defaultValue",	JSP_HTML_TEXT_AREA_ELEMENT_DEFAULT_VALUE,	JSPROP_ENUMERATE },
	{ "form",	JSP_HTML_TEXT_AREA_ELEMENT_FORM,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "accessKey",	JSP_HTML_TEXT_AREA_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "cols",	JSP_HTML_TEXT_AREA_ELEMENT_COLS,	JSPROP_ENUMERATE },
	{ "disabled",	JSP_HTML_TEXT_AREA_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "name",	JSP_HTML_TEXT_AREA_ELEMENT_NAME,	JSPROP_ENUMERATE },
	{ "readOnly",	JSP_HTML_TEXT_AREA_ELEMENT_READ_ONLY,	JSPROP_ENUMERATE },
	{ "rows",	JSP_HTML_TEXT_AREA_ELEMENT_ROWS,	JSPROP_ENUMERATE },
	{ "tabIndex",	JSP_HTML_TEXT_AREA_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_TEXT_AREA_ELEMENT_TYPE,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "value",	JSP_HTML_TEXT_AREA_ELEMENT_DEFAULT_VALUE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLTextAreaElement_funcs[] = {
	{ "blur",	HTMLTextAreaElement_blur,	0 },
	{ "focus",	HTMLTextAreaElement_focus,	0 },
	{ "select",	HTMLTextAreaElement_select,	0 },
	{ NULL }
};

const JSClass HTMLTextAreaElement_class = {
	"HTMLTextAreaElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLTextAreaElement_getProperty, HTMLTextAreaElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};
