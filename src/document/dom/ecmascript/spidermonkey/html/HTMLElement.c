#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Element.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "dom/node.h"
#include "util/hash.h"

JSBool
HTMLElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct HTMLElement_struct *html;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;
	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ELEMENT_ID:
		string_to_jsval(ctx, vp, html->id);
		break;
	case JSP_HTML_ELEMENT_TITLE:
		string_to_jsval(ctx, vp, html->title);
		break;
	case JSP_HTML_ELEMENT_LANG:
		string_to_jsval(ctx, vp, html->lang);
		break;
	case JSP_HTML_ELEMENT_DIR:
		string_to_jsval(ctx, vp, html->dir);
		break;
	case JSP_HTML_ELEMENT_CLASS_NAME:
		string_to_jsval(ctx, vp, html->class_name);
		break;
	default:
		return Element_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

JSBool
HTMLElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct HTMLElement_struct *html;
	struct html_objects *o;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLElement_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.element.html_data;
	if (!html)
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_ELEMENT_ID:
		o = JS_GetContextPrivate(ctx);
		if (html->id) {
			struct hash_item *item = get_hash_item(o->ids, html->id, strlen(html->id));

			if (item)
				del_hash_item(o->ids, item);
		}
		mem_free_set(&html->id, stracpy(jsval_to_string(ctx, vp)));
		if (html->id)
			add_hash_item(o->ids, html->id, strlen(html->id), node);
		break;
	case JSP_HTML_ELEMENT_TITLE:
		mem_free_set(&html->title, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ELEMENT_LANG:
		mem_free_set(&html->lang, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ELEMENT_DIR:
		mem_free_set(&html->dir, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_ELEMENT_CLASS_NAME:
		mem_free_set(&html->class_name, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return Node_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

const JSPropertySpec HTMLElement_props[] = {
	{ "id",		JSP_HTML_ELEMENT_ID,		JSPROP_ENUMERATE },
	{ "title",	JSP_HTML_ELEMENT_TITLE,		JSPROP_ENUMERATE },
	{ "lang",	JSP_HTML_ELEMENT_LANG,		JSPROP_ENUMERATE },
	{ "dir",	JSP_HTML_ELEMENT_DIR,		JSPROP_ENUMERATE },
	{ "className",	JSP_HTML_ELEMENT_CLASS_NAME,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLElement_funcs[] = {
	{ NULL }
};

const JSClass HTMLElement_class = {
	"HTMLElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLElement_getProperty, HTMLElement_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

void
make_HTMLElement(JSContext *ctx, struct dom_node *node)
{
	struct html_objects *o = JS_GetContextPrivate(ctx);

	node->data.element.html_data = mem_calloc(1, sizeof(struct HTMLElement_struct));
	if (node->data.element.html_data) {
		node->ecmascript_obj = JS_NewObject(ctx, (JSClass *)&HTMLElement_class, o->Element_object, NULL);
		o->HTMLElement_object = node->ecmascript_obj;
	}
}

void
done_HTMLElement(struct dom_node *node)
{
	struct HTMLElement_struct *d = node->data.element.html_data;
	JSContext *ctx = node->ecmascript_ctx;
	struct html_objects *o = JS_GetContextPrivate(ctx);

	if (d->id) {
		struct hash_item *item = get_hash_item(o->ids, d->id, strlen(d->id));

		if (item)
			del_hash_item(o->ids, item);
		mem_free(d->id);
	}
	mem_free_if(d->title);
	mem_free_if(d->lang);
	mem_free_if(d->dir);
	mem_free_if(d->class_name);
}
