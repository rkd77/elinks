#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLAppletElement.h"
#include "dom/node.h"

static JSBool
HTMLAppletElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct APPLET_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLAppletElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_APPLET_ELEMENT_ALIGN:
		string_to_jsval(ctx, vp, html->align);
		break;
	case JSP_HTML_APPLET_ELEMENT_ALT:
		string_to_jsval(ctx, vp, html->alt);
		break;
	case JSP_HTML_APPLET_ELEMENT_ARCHIVE:
		string_to_jsval(ctx, vp, html->archive);
		break;
	case JSP_HTML_APPLET_ELEMENT_CODE:
		string_to_jsval(ctx, vp, html->code);
		break;
	case JSP_HTML_APPLET_ELEMENT_CODE_BASE:
		string_to_jsval(ctx, vp, html->code_base);
		break;
	case JSP_HTML_APPLET_ELEMENT_HEIGHT:
		string_to_jsval(ctx, vp, html->height);
		break;
	case JSP_HTML_APPLET_ELEMENT_HSPACE:
		string_to_jsval(ctx, vp, html->hspace);
		break;
	case JSP_HTML_APPLET_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_APPLET_ELEMENT_OBJECT:
		string_to_jsval(ctx, vp, html->object);
		break;
	case JSP_HTML_APPLET_ELEMENT_VSPACE:
		string_to_jsval(ctx, vp, html->vspace);
		break;
	case JSP_HTML_APPLET_ELEMENT_WIDTH:
		string_to_jsval(ctx, vp, html->width);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLAppletElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct APPLET_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLAppletElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_APPLET_ELEMENT_ALIGN:
		mem_free_set(&html->align, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_ALT:
		mem_free_set(&html->alt, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_ARCHIVE:
		mem_free_set(&html->archive, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_CODE:
		mem_free_set(&html->code, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_CODE_BASE:
		mem_free_set(&html->code_base, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_HEIGHT:
		mem_free_set(&html->height, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_HSPACE:
		mem_free_set(&html->hspace, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_OBJECT:
		mem_free_set(&html->object, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_VSPACE:
		mem_free_set(&html->vspace, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_APPLET_ELEMENT_WIDTH:
		mem_free_set(&html->width, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLAppletElement_props[] = {
	{ "align",	JSP_HTML_APPLET_ELEMENT_ALIGN,		JSPROP_ENUMERATE },
	{ "alt",	JSP_HTML_APPLET_ELEMENT_ALT,		JSPROP_ENUMERATE },
	{ "archive",	JSP_HTML_APPLET_ELEMENT_ARCHIVE,	JSPROP_ENUMERATE },
	{ "code",	JSP_HTML_APPLET_ELEMENT_CODE,		JSPROP_ENUMERATE },
	{ "codeBase",	JSP_HTML_APPLET_ELEMENT_CODE_BASE,	JSPROP_ENUMERATE },
	{ "height",	JSP_HTML_APPLET_ELEMENT_HEIGHT,		JSPROP_ENUMERATE },
	{ "hspace",	JSP_HTML_APPLET_ELEMENT_HSPACE,		JSPROP_ENUMERATE },
	{ "name",	JSP_HTML_APPLET_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "object",	JSP_HTML_APPLET_ELEMENT_OBJECT,		JSPROP_ENUMERATE },
	{ "vspace",	JSP_HTML_APPLET_ELEMENT_VSPACE,		JSPROP_ENUMERATE },
	{ "width",	JSP_HTML_APPLET_ELEMENT_WIDTH,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLAppletElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLAppletElement_class = {
	"HTMLAppletElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLAppletElement_getProperty, HTMLAppletElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};
