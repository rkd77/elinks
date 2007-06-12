#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLHRElement.h"
#include "dom/node.h"

static JSBool
HTMLHRElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct HR_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLHRElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_HR_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_HR_ELEMENT_NO_SHADE:
		boolean_to_jsval(ctx, vp, html->no_shade);
		break;
	case JSP_HTML_HR_ELEMENT_SIZE:
		string_to_jsval(ctx, vp, html->size);
		break;
	case JSP_HTML_HR_ELEMENT_WIDTH:
		string_to_jsval(ctx, vp, html->width);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLHRElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct HR_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLHRElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_HR_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_HR_ELEMENT_NO_SHADE:
		html->no_shade = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_HR_ELEMENT_SIZE:
		mem_free_set(&html->size, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_HR_ELEMENT_WIDTH:
		mem_free_set(&html->width, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLHRElement_props[] = {
	{ "align",	JSP_HTML_HR_ELEMENT_ALIGN,	JSPROP_ENUMERATE },
	{ "noShade",	JSP_HTML_HR_ELEMENT_NO_SHADE,	JSPROP_ENUMERATE },
	{ "size",	JSP_HTML_HR_ELEMENT_SIZE,	JSPROP_ENUMERATE },
	{ "width",	JSP_HTML_HR_ELEMENT_WIDTH,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLHRElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLHRElement_class = {
	"HTMLHRElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLHRElement_getProperty, HTMLHRElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_HR_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct HR_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLHRElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_HR_object(struct dom_node *node)
{
	struct HR_struct *d = node->data.element.html_data;

	mem_free_if(d->align);
	mem_free_if(d->size);
	mem_free_if(d->width);
}
