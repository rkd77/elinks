#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFormElement.h"
#include "dom/node.h"

static JSBool
HTMLFormElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct FORM_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLFormElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FORM_ELEMENT_ELEMENTS:
		string_to_jsval(ctx, vp, html->elements);
		/* Write me! */
		break;
	case JSP_HTML_FORM_ELEMENT_LENGTH:
		int_to_jsval(ctx, vp, html->length);
		break;
	case JSP_HTML_FORM_ELEMENT_NAME:
		string_to_jsval(ctx, vp, html->name);
		break;
	case JSP_HTML_FORM_ELEMENT_ACCEPT_CHARSET:
		string_to_jsval(ctx, vp, html->accept_charset);
		break;
	case JSP_HTML_FORM_ELEMENT_ACTION:
		string_to_jsval(ctx, vp, html->action);
		break;
	case JSP_HTML_FORM_ELEMENT_ENCTYPE:
		string_to_jsval(ctx, vp, html->enctype);
		break;
	case JSP_HTML_FORM_ELEMENT_METHOD:
		string_to_jsval(ctx, vp, html->method);
		break;
	case JSP_HTML_FORM_ELEMENT_TARGET:
		string_to_jsval(ctx, vp, html->target);
		break;
	default:
		return HTMLElement_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLFormElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct FORM_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLFormElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;
	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_FORM_ELEMENT_NAME:
		mem_free_set(&html->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FORM_ELEMENT_ACCEPT_CHARSET:
		mem_free_set(&html->accept_charset, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FORM_ELEMENT_ACTION:
		mem_free_set(&html->action, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FORM_ELEMENT_ENCTYPE:
		mem_free_set(&html->enctype, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FORM_ELEMENT_METHOD:
		mem_free_set(&html->method, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_FORM_ELEMENT_TARGET:
		mem_free_set(&html->target, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return HTMLElement_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLFormElement_submit(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLFormElement_reset(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLFormElement_props[] = {
	{ "elements",		JSP_HTML_FORM_ELEMENT_ELEMENTS,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "length",		JSP_HTML_FORM_ELEMENT_LENGTH,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "name",		JSP_HTML_FORM_ELEMENT_NAME,		JSPROP_ENUMERATE },
	{ "acceptCharset",	JSP_HTML_FORM_ELEMENT_ACCEPT_CHARSET,	JSPROP_ENUMERATE },
	{ "action",		JSP_HTML_FORM_ELEMENT_ACTION,		JSPROP_ENUMERATE },
	{ "enctype",		JSP_HTML_FORM_ELEMENT_ENCTYPE,		JSPROP_ENUMERATE },
	{ "method",		JSP_HTML_FORM_ELEMENT_METHOD,	JSPROP_ENUMERATE },
	{ "target",		JSP_HTML_FORM_ELEMENT_TARGET,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLFormElement_funcs[] = {
	{ "submit",	HTMLFormElement_submit,	0 },
	{ "reset",	HTMLFormElement_reset,	0 },
	{ NULL }
};

const JSClass HTMLFormElement_class = {
	"HTMLFormElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLFormElement_getProperty, HTMLFormElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_FORM_object(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct FORM_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLFormElement_class, o->HTMLElement_object, NULL);
	}
}

void
done_FORM_object(void *data)
{
	struct FORM_struct *d = data;

	/* elements */
	mem_free_if(d->name);
	mem_free_if(d->accept_charset);
	mem_free_if(d->action);
	mem_free_if(d->enctype);
	mem_free_if(d->method);
	mem_free_if(d->target);
}
