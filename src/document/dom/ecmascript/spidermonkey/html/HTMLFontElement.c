#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFontElement.h"
#include "dom/node.h"

static JSBool
HTMLFontElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct FONT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLFontElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FONT_ELEMENT_COLOR:
		string_to_jsval(ctx, vp, html->color);
		break;
	case JSP_HTML_FONT_ELEMENT_FACE:
		string_to_jsval(ctx, vp, html->face);
		break;
	case JSP_HTML_FONT_ELEMENT_SIZE:
		string_to_jsval(ctx, vp, html->size);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLFontElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct FONT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLFontElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FONT_ELEMENT_COLOR:
		mem_free_set(&html->color, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FONT_ELEMENT_FACE:
		mem_free_set(&html->face, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FONT_ELEMENT_SIZE:
		mem_free_set(&html->size, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLFontElement_props[] = {
	{ "color",	JSP_HTML_FONT_ELEMENT_COLOR,	JSPROP_ENUMERATE },
	{ "face",	JSP_HTML_FONT_ELEMENT_FACE,	JSPROP_ENUMERATE },
	{ "size",	JSP_HTML_FONT_ELEMENT_SIZE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLFontElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLFontElement_class = {
	"HTMLFontElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLFontElement_getProperty, HTMLFontElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_FONT_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct FONT_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLFontElement_class, o->HTMLElement_object, NULL);
	}
}

