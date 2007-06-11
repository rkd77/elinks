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
		if (html->form)
			object_to_jsval(ctx, vp, html->form->ecmascript_obj);
		else
			undef_to_jsval(ctx, vp);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ACCESS_KEY:
		string_to_jsval(ctx, vp, html->access_key);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_COLS:
		int_to_jsval(ctx, vp, html->cols);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_DISABLED:
		boolean_to_jsval(ctx, vp, html->disabled);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_READ_ONLY:
		boolean_to_jsval(ctx, vp, html->read_only);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ROWS:
		int_to_jsval(ctx, vp, html->rows);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_TAB_INDEX:
		int_to_jsval(ctx, vp, html->tab_index);
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
		return JS_ValueToInt32(ctx, *vp, &html->cols);
	case JSP_HTML_TEXT_AREA_ELEMENT_DISABLED:
		html->disabled = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_READ_ONLY:
		html->read_only = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_TEXT_AREA_ELEMENT_ROWS:
		return JS_ValueToInt32(ctx, *vp, &html->rows);
	case JSP_HTML_TEXT_AREA_ELEMENT_TAB_INDEX:
		return JS_ValueToInt32(ctx, *vp, &html->tab_index);
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

void
make_TEXTAREA_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct TEXTAREA_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLTextAreaElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_TEXTAREA_object(void *data)
{
	struct TEXTAREA_struct *d = data;

	mem_free_if(d->default_value);
	/* What to do with d->form? */
	mem_free_if(d->access_key);
	mem_free_if(d->name);
	mem_free_if(d->type);
	mem_free_if(d->value);
}
