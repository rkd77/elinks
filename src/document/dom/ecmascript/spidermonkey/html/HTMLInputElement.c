#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLInputElement.h"
#include "dom/node.h"

static JSBool
HTMLInputElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct INPUT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLInputElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_INPUT_ELEMENT_DEFAULT_VALUE:
		string_to_jsval(ctx, vp, html->default_value);
		break;
	case JSP_HTML_INPUT_ELEMENT_DEFAULT_CHECKED:
		boolean_to_jsval(ctx, vp, html->default_checked);
		break;
	case JSP_HTML_INPUT_ELEMENT_FORM:
		if (html->form)
			object_to_jsval(ctx, vp, html->form->ecmascript_obj);
		else
			undef_to_jsval(ctx, vp);
		break;
	case JSP_HTML_INPUT_ELEMENT_ACCEPT:
		string_to_jsval(ctx, vp, html->accept);
		break;
	case JSP_HTML_INPUT_ELEMENT_ACCESS_KEY:
		string_to_jsval(ctx, vp, html->access_key);
		break;
	case JSP_HTML_INPUT_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_INPUT_ELEMENT_ALT:
		string_to_jsval(ctx, vp, html->alt);
		break;
	case JSP_HTML_INPUT_ELEMENT_CHECKED:
		boolean_to_jsval(ctx, vp, html->checked);
		break;
	case JSP_HTML_INPUT_ELEMENT_DISABLED:
		boolean_to_jsval(ctx, vp, html->disabled);
		break;
	case JSP_HTML_INPUT_ELEMENT_MAX_LENGTH:
		int_to_jsval(ctx, vp, html->max_length);
		break;
	case JSP_HTML_INPUT_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_INPUT_ELEMENT_READ_ONLY:
		boolean_to_jsval(ctx, vp, html->read_only);
		break;
	case JSP_HTML_INPUT_ELEMENT_SIZE:
		int_to_jsval(ctx, vp, html->size);
		break;
	case JSP_HTML_INPUT_ELEMENT_SRC:
		string_to_jsval(ctx, vp, html->src);
		break;
	case JSP_HTML_INPUT_ELEMENT_TAB_INDEX:
		int_to_jsval(ctx, vp, html->tab_index);
		break;
	case JSP_HTML_INPUT_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	case JSP_HTML_INPUT_ELEMENT_USE_MAP:
		string_to_jsval(ctx, vp, html->use_map);
		break;
	case JSP_HTML_INPUT_ELEMENT_VALUE:
		string_to_jsval(ctx, vp, html->value);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLInputElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct INPUT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLInputElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_INPUT_ELEMENT_DEFAULT_VALUE:
		mem_free_set(&html->default_value, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_INPUT_ELEMENT_DEFAULT_CHECKED:
		html->default_checked = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_INPUT_ELEMENT_ACCEPT:
		mem_free_set(&html->accept, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_INPUT_ELEMENT_ACCESS_KEY:
		mem_free_set(&html->access_key, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_INPUT_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_INPUT_ELEMENT_ALT:
		mem_free_set(&html->alt, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_INPUT_ELEMENT_CHECKED:
		html->checked = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_INPUT_ELEMENT_DISABLED:
		html->disabled = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_INPUT_ELEMENT_MAX_LENGTH:
		return JS_ValueToInt32(ctx, *vp, &html->max_length);
	case JSP_HTML_INPUT_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_INPUT_ELEMENT_READ_ONLY:
		html->read_only = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_INPUT_ELEMENT_SIZE:
		return JS_ValueToInt32(ctx, *vp, &html->size);
	case JSP_HTML_INPUT_ELEMENT_SRC:
		mem_free_set(&html->src, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_INPUT_ELEMENT_TAB_INDEX:
		return JS_ValueToInt32(ctx, *vp, &html->tab_index);
	case JSP_HTML_INPUT_ELEMENT_TYPE:
		mem_free_set(&html->type, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_INPUT_ELEMENT_USE_MAP:
		mem_free_set(&html->use_map, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_INPUT_ELEMENT_VALUE:
		mem_free_set(&html->value, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLInputElement_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLInputElement_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLInputElement_select(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLInputElement_click(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLInputElement_props[] = {
	{ "defaultValue",	JSP_HTML_INPUT_ELEMENT_DEFAULT_VALUE,	JSPROP_ENUMERATE },
	{ "defaultChecked",	JSP_HTML_INPUT_ELEMENT_DEFAULT_CHECKED,	JSPROP_ENUMERATE },
	{ "form",		JSP_HTML_INPUT_ELEMENT_FORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "accept",		JSP_HTML_INPUT_ELEMENT_ACCEPT,		JSPROP_ENUMERATE },
	{ "accessKey",		JSP_HTML_INPUT_ELEMENT_ACCESS_KEY,	JSPROP_ENUMERATE },
	{ "align",		JSP_HTML_INPUT_ELEMENT_ALIGN,		JSPROP_ENUMERATE },
	{ "alt",		JSP_HTML_INPUT_ELEMENT_ALT,		JSPROP_ENUMERATE },
	{ "checked",		JSP_HTML_INPUT_ELEMENT_CHECKED,		JSPROP_ENUMERATE },
	{ "disabled",		JSP_HTML_INPUT_ELEMENT_DISABLED,	JSPROP_ENUMERATE },
	{ "maxLength",		JSP_HTML_INPUT_ELEMENT_MAX_LENGTH,	JSPROP_ENUMERATE },
	{ "name",		JSP_HTML_INPUT_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "readOnly",		JSP_HTML_INPUT_ELEMENT_READ_ONLY,	JSPROP_ENUMERATE },
	{ "size",		JSP_HTML_INPUT_ELEMENT_SIZE,		JSPROP_ENUMERATE },
	{ "src",		JSP_HTML_INPUT_ELEMENT_SRC,		JSPROP_ENUMERATE },
	{ "tabIndex",		JSP_HTML_INPUT_ELEMENT_TAB_INDEX,	JSPROP_ENUMERATE },
	{ "type",		JSP_HTML_INPUT_ELEMENT_TYPE,		JSPROP_ENUMERATE },
	{ "useMap",		JSP_HTML_INPUT_ELEMENT_USE_MAP,		JSPROP_ENUMERATE },
	{ "value",		JSP_HTML_INPUT_ELEMENT_VALUE,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLInputElement_funcs[] = {
	{ "blur",	HTMLInputElement_blur,		0 },
	{ "focus",	HTMLInputElement_focus,		0 },
	{ "select",	HTMLInputElement_select,	0 },
	{ "click",	HTMLInputElement_click,		0 },
	{ NULL }
};

const JSClass HTMLInputElement_class = {
	"HTMLInputElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLInputElement_getProperty, HTMLInputElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_INPUT_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct INPUT_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLInputElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_INPUT_object(struct dom_node *node)
{
	struct INPUT_struct *d = node->data.element.html_data;

	mem_free_if(d->default_value);
	/* form ? */
	mem_free_if(d->accept);
	mem_free_if(d->access_key);
	mem_free_if(d->align);
	mem_free_if(d->alt);
	mem_free_if(d->name);
	mem_free_if(d->src);
	mem_free_if(d->type);
	mem_free_if(d->use_map);
	mem_free_if(d->value);
}
