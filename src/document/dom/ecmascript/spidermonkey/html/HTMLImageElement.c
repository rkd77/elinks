#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLImageElement.h"
#include "dom/node.h"

static JSBool
HTMLImageElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct IMAGE_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLImageElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IMAGE_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_IMAGE_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_IMAGE_ELEMENT_ALT:
		string_to_jsval(ctx, vp, html->alt);
		break;
	case JSP_HTML_IMAGE_ELEMENT_BORDER:
		string_to_jsval(ctx, vp, html->border);
		break;
	case JSP_HTML_IMAGE_ELEMENT_HEIGHT:
		int_to_jsval(ctx, vp, html->height);
		break;
	case JSP_HTML_IMAGE_ELEMENT_HSPACE:
		int_to_jsval(ctx, vp, html->hspace);
		break;
	case JSP_HTML_IMAGE_ELEMENT_IS_MAP:
		boolean_to_jsval(ctx, vp, html->is_map);
		break;
	case JSP_HTML_IMAGE_ELEMENT_LONG_DESC:
		string_to_jsval(ctx, vp, html->long_desc);
		break;
	case JSP_HTML_IMAGE_ELEMENT_SRC:
		string_to_jsval(ctx, vp, html->src);
		break;
	case JSP_HTML_IMAGE_ELEMENT_USE_MAP:
		string_to_jsval(ctx, vp, html->use_map);
		break;
	case JSP_HTML_IMAGE_ELEMENT_VSPACE:
		int_to_jsval(ctx, vp, html->vspace);
		break;
	case JSP_HTML_IMAGE_ELEMENT_WIDTH:
		int_to_jsval(ctx, vp, html->width);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLImageElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct IMAGE_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLImageElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_IMAGE_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IMAGE_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IMAGE_ELEMENT_ALT:
		mem_free_set(&html->alt, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IMAGE_ELEMENT_BORDER:
		mem_free_set(&html->border, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IMAGE_ELEMENT_HEIGHT:
		return JS_ValueToInt32(ctx, *vp, &html->height);
	case JSP_HTML_IMAGE_ELEMENT_HSPACE:
		return JS_ValueToInt32(ctx, *vp, &html->hspace);
	case JSP_HTML_IMAGE_ELEMENT_IS_MAP:
		html->is_map = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_IMAGE_ELEMENT_LONG_DESC:
		mem_free_set(&html->long_desc, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IMAGE_ELEMENT_SRC:
		mem_free_set(&html->src, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IMAGE_ELEMENT_USE_MAP:
		mem_free_set(&html->use_map, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_IMAGE_ELEMENT_VSPACE:
		return JS_ValueToInt32(ctx, *vp, &html->vspace);
	case JSP_HTML_IMAGE_ELEMENT_WIDTH:
		return JS_ValueToInt32(ctx, *vp, &html->width);
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLImageElement_props[] = {
	{ "name",	JSP_HTML_IMAGE_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "align",	JSP_HTML_IMAGE_ELEMENT_ALIGN,		JSPROP_ENUMERATE },
	{ "alt",	JSP_HTML_IMAGE_ELEMENT_ALT,		JSPROP_ENUMERATE },
	{ "border",	JSP_HTML_IMAGE_ELEMENT_BORDER,		JSPROP_ENUMERATE },
	{ "height",	JSP_HTML_IMAGE_ELEMENT_HEIGHT,		JSPROP_ENUMERATE },
	{ "hspace",	JSP_HTML_IMAGE_ELEMENT_HSPACE,		JSPROP_ENUMERATE },
	{ "isMap",	JSP_HTML_IMAGE_ELEMENT_IS_MAP,		JSPROP_ENUMERATE },
	{ "longDesc",	JSP_HTML_IMAGE_ELEMENT_LONG_DESC,	JSPROP_ENUMERATE },
	{ "src",	JSP_HTML_IMAGE_ELEMENT_SRC,		JSPROP_ENUMERATE },
	{ "useMap",	JSP_HTML_IMAGE_ELEMENT_USE_MAP,		JSPROP_ENUMERATE },
	{ "vspace",	JSP_HTML_IMAGE_ELEMENT_VSPACE,		JSPROP_ENUMERATE },
	{ "width",	JSP_HTML_IMAGE_ELEMENT_WIDTH,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLImageElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLImageElement_class = {
	"HTMLImageElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLImageElement_getProperty, HTMLImageElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_IMAGE_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct IMAGE_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLImageElement_class, o->HTMLElement_object, NULL);
	}
}

