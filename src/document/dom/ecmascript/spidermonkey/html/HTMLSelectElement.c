#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLSelectElement.h"
#include "dom/node.h"

static JSBool
HTMLSelectElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct SELECT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLSelectElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_SELECT_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	case JSP_HTML_SELECT_ELEMENT_SELECTED_INDEX:
		int_to_jsval(ctx, vp, html->selected_index);
		break;
	case JSP_HTML_SELECT_ELEMENT_VALUE:
		string_to_jsval(ctx, vp, html->value);
		break;
	case JSP_HTML_SELECT_ELEMENT_LENGTH:
		int_to_jsval(ctx, vp, html->length);
		break;
	case JSP_HTML_SELECT_ELEMENT_FORM:
		string_to_jsval(ctx, vp, html->form);
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_OPTIONS:
		string_to_jsval(ctx, vp, html->options);
		/* Write me! */
		break;
	case JSP_HTML_SELECT_ELEMENT_DISABLED:
		boolean_to_jsval(ctx, vp, html->disabled);
		break;
	case JSP_HTML_SELECT_ELEMENT_MULTIPLE:
		boolean_to_jsval(ctx, vp, html->multiple);
		break;
	case JSP_HTML_SELECT_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_SELECT_ELEMENT_SIZE:
		int_to_jsval(ctx, vp, html->size);
		break;
	case JSP_HTML_SELECT_ELEMENT_TAB_INDEX:
		int_to_jsval(ctx, vp, html->tab_index);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct SELECT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLSelectElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_SELECT_ELEMENT_SELECTED_INDEX:
		return JS_ValueToInt32(ctx, *vp, &html->selected_index);
	case JSP_HTML_SELECT_ELEMENT_VALUE:
		mem_free_set(&html->value, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_SELECT_ELEMENT_LENGTH:
		return JS_ValueToInt32(ctx, *vp, &html->length);
	case JSP_HTML_SELECT_ELEMENT_DISABLED:
		html->disabled = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_SELECT_ELEMENT_MULTIPLE:
		html->multiple = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_SELECT_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_SELECT_ELEMENT_SIZE:
		return JS_ValueToInt32(ctx, *vp, &html->size);
	case JSP_HTML_SELECT_ELEMENT_TAB_INDEX:
		return JS_ValueToInt32(ctx, *vp, &html->tab_index);
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_add(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_remove(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLSelectElement_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLSelectElement_props[] = {
	{ "type",		JSP_HTML_SELECT_ELEMENT_TYPE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "selectedIndex",	JSP_HTML_SELECT_ELEMENT_SELECTED_INDEX,	JSPROP_ENUMERATE },
	{ "value",		JSP_HTML_SELECT_ELEMENT_VALUE,		JSPROP_ENUMERATE },
	{ "length",		JSP_HTML_SELECT_ELEMENT_LENGTH,		JSPROP_ENUMERATE },
	{ "form",		JSP_HTML_SELECT_ELEMENT_FORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "options",		JSP_HTML_SELECT_ELEMENT_OPTIONS,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "disabled",		JSP_HTML_SELECT_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "multiple",		JSP_HTML_SELECT_ELEMENT_MULTIPLE,	JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_SELECT_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "size",		JSP_HTML_SELECT_ELEMENT_SIZE,		JSPROP_ENUMERATE },
	{ "tabIndex",		JSP_HTML_SELECT_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLSelectElement_funcs[] = {
	{ "add",	HTMLSelectElement_add,		1 },
	{ "remove",	HTMLSelectElement_remove,	2 },
	{ "blur",	HTMLSelectElement_blur,		0 },
	{ "focus",	HTMLSelectElement_focus,	0 },
	{ NULL }
};

const JSClass HTMLSelectElement_class = {
	"HTMLSelectElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLSelectElement_getProperty, HTMLSelectElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_SELECT_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct SELECT_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLSelectElement_class, o->HTMLElement_object, NULL);
	}
}

