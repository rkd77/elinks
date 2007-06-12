#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLScriptElement.h"
#include "dom/node.h"

static JSBool
HTMLScriptElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct SCRIPT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLScriptElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_SCRIPT_ELEMENT_TEXT:
		string_to_jsval(ctx, vp, html->text);
		break;
	case JSP_HTML_SCRIPT_ELEMENT_HTML_FOR:
		string_to_jsval(ctx, vp, html->html_for);
		break;
	case JSP_HTML_SCRIPT_ELEMENT_EVENT:
		string_to_jsval(ctx, vp, html->event);
		break;
	case JSP_HTML_SCRIPT_ELEMENT_CHARSET:
		string_to_jsval(ctx, vp, html->charset);
		break;
	case JSP_HTML_SCRIPT_ELEMENT_DEFER:
		boolean_to_jsval(ctx, vp, html->defer);
		break;
	case JSP_HTML_SCRIPT_ELEMENT_SRC:
		string_to_jsval(ctx, vp, html->src);
		break;
	case JSP_HTML_SCRIPT_ELEMENT_TYPE:
		string_to_jsval(ctx, vp, html->type);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLScriptElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct SCRIPT_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLScriptElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_SCRIPT_ELEMENT_TEXT:
		mem_free_set(&html->text, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_SCRIPT_ELEMENT_HTML_FOR:
		mem_free_set(&html->html_for, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_SCRIPT_ELEMENT_EVENT:
		mem_free_set(&html->event, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_SCRIPT_ELEMENT_CHARSET:
		mem_free_set(&html->charset, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_SCRIPT_ELEMENT_DEFER:
		html->defer = jsval_to_boolean(ctx, vp);
		break;
	case JSP_HTML_SCRIPT_ELEMENT_SRC:
		mem_free_set(&html->src, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_SCRIPT_ELEMENT_TYPE:
		mem_free_set(&html->type, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLScriptElement_props[] = {
	{ "text",	JSP_HTML_SCRIPT_ELEMENT_TEXT,		JSPROP_ENUMERATE },
	{ "htmlFor",	JSP_HTML_SCRIPT_ELEMENT_HTML_FOR,	JSPROP_ENUMERATE },
	{ "event",	JSP_HTML_SCRIPT_ELEMENT_EVENT	,	JSPROP_ENUMERATE },
	{ "charset",	JSP_HTML_SCRIPT_ELEMENT_CHARSET,	JSPROP_ENUMERATE },
	{ "defer",	JSP_HTML_SCRIPT_ELEMENT_DEFER,		JSPROP_ENUMERATE },
	{ "src",	JSP_HTML_SCRIPT_ELEMENT_SRC,		JSPROP_ENUMERATE },
	{ "type",	JSP_HTML_SCRIPT_ELEMENT_TYPE,		JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLScriptElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLScriptElement_class = {
	"HTMLScriptElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLScriptElement_getProperty, HTMLScriptElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_SCRIPT_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct SCRIPT_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLScriptElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_SCRIPT_object(struct dom_node *node)
{
	struct SCRIPT_struct *d = node->data.element.html_data;

	mem_free_if(d->text);
	mem_free_if(d->html_for);
	mem_free_if(d->event);
	mem_free_if(d->charset);
	mem_free_if(d->src);
	mem_free_if(d->type);
}
