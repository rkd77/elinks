#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/dom/ecmascript/spidermonkey.h"
#include "document/dom/ecmascript/spidermonkey/Node.h"
#include "document/dom/ecmascript/spidermonkey/Document.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLCollection.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLDocument.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "dom/node.h"

static JSBool
HTMLDocument_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct HTMLDocument_struct *html;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLDocument_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.document.html_data;
	if (!html)
		return JS_FALSE;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_DOCUMENT_TITLE:
		string_to_jsval(ctx, vp, html->title);
		break;
	case JSP_HTML_DOCUMENT_REFERER:
		string_to_jsval(ctx, vp, html->referrer);
		break;
	case JSP_HTML_DOCUMENT_DOMAIN:
		string_to_jsval(ctx, vp, html->domain);
		break;
	case JSP_HTML_DOCUMENT_URL:
		string_to_jsval(ctx, vp, html->URL);
		break;
	case JSP_HTML_DOCUMENT_BODY:
		if (html->body) {
			if (ctx == html->body->ecmascript_ctx)
				object_to_jsval(ctx, vp, html->body->ecmascript_obj);
		}
		break;
	case JSP_HTML_DOCUMENT_IMAGES:
		if (!html->images)
			undef_to_jsval(ctx, vp);
		else {
			if (html->images->ctx == ctx)
				object_to_jsval(ctx, vp, html->images->ecmascript_obj);
			else {
				JSObject *new_obj;

				html->images->ctx = ctx;
				new_obj = JS_NewObject(ctx,
				 (JSClass *)&HTMLCollection_class, NULL, NULL);
				if (new_obj)
					JS_SetPrivate(ctx, new_obj, html->images);
				html->images->ecmascript_obj = new_obj;
				object_to_jsval(ctx, vp, new_obj);
			}
		}
		break;
	case JSP_HTML_DOCUMENT_APPLETS:
		if (!html->applets)
			undef_to_jsval(ctx, vp);
		else {
			if (html->images->ctx == ctx)
				object_to_jsval(ctx, vp, html->applets->ecmascript_obj);
			else {
				JSObject *new_obj;

				html->applets->ctx = ctx;
				new_obj = JS_NewObject(ctx,
				 (JSClass *)&HTMLCollection_class, NULL, NULL);
				if (new_obj)
					JS_SetPrivate(ctx, new_obj, html->applets);
				html->applets->ecmascript_obj = new_obj;
				object_to_jsval(ctx, vp, new_obj);
			}
		}
		break;
	case JSP_HTML_DOCUMENT_LINKS:
		if (!html->links)
			undef_to_jsval(ctx, vp);
		else {
			if (html->links->ctx == ctx)
				object_to_jsval(ctx, vp, html->links->ecmascript_obj);
			else {
				JSObject *new_obj;

				html->links->ctx = ctx;
				new_obj = JS_NewObject(ctx,
				 (JSClass *)&HTMLCollection_class, NULL, NULL);
				if (new_obj)
					JS_SetPrivate(ctx, new_obj, html->links);
				html->links->ecmascript_obj = new_obj;
				object_to_jsval(ctx, vp, new_obj);
			}
		}
		break;
	case JSP_HTML_DOCUMENT_FORMS:
		if (!html->forms)
			undef_to_jsval(ctx, vp);
		else {
			if (html->forms->ctx == ctx)
				object_to_jsval(ctx, vp, html->forms->ecmascript_obj);
			else {
				JSObject *new_obj;

				html->forms->ctx = ctx;
				new_obj = JS_NewObject(ctx,
				 (JSClass *)&HTMLCollection_class, NULL, NULL);
				if (new_obj)
					JS_SetPrivate(ctx, new_obj, html->forms);
				html->forms->ecmascript_obj = new_obj;
				object_to_jsval(ctx, vp, new_obj);
			}
		}
		break;
	case JSP_HTML_DOCUMENT_ANCHORS:
		if (!html->anchors)
			undef_to_jsval(ctx, vp);
		else {
			if (html->anchors->ctx == ctx)
				object_to_jsval(ctx, vp, html->anchors->ecmascript_obj);
			else {
				JSObject *new_obj;

				html->anchors->ctx = ctx;
				new_obj = JS_NewObject(ctx,
				 (JSClass *)&HTMLCollection_class, NULL, NULL);
				if (new_obj)
					JS_SetPrivate(ctx, new_obj, html->anchors);
				html->anchors->ecmascript_obj = new_obj;
				object_to_jsval(ctx, vp, new_obj);
			}
		}
		break;
	case JSP_HTML_DOCUMENT_COOKIE:
		string_to_jsval(ctx, vp, html->cookie);
		break;
	default:
		return Document_getProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLDocument_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct dom_node *node;
	struct HTMLDocument_struct *html;
	JSObject *value;

	if (!obj || (!JS_InstanceOf(ctx, obj, (JSClass *)&HTMLDocument_class, NULL)))
		return JS_FALSE;

	node = JS_GetPrivate(ctx, obj);
	if (!node)
		return JS_FALSE;
	html = node->data.document.html_data;
	if (!html)
		return JS_FALSE;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_HTML_DOCUMENT_TITLE:
		mem_free_set(&html->title, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_HTML_DOCUMENT_BODY:
		value = JSVAL_TO_OBJECT(*vp);
		if (value) {
			struct dom_node *body = JS_GetPrivate(ctx, value);

			html->body = body;
		}
		break;
	case JSP_HTML_DOCUMENT_COOKIE:
		mem_free_set(&html->cookie, stracpy(jsval_to_string(ctx, vp)));
		break;
	default:
		return Document_setProperty(ctx, obj, id, vp);
	}
	return JS_TRUE;
}

static JSBool
HTMLDocument_open(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLDocument_close(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLDocument_write(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLDocument_writeln(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

static JSBool
HTMLDocument_getElementsByName(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
		jsval *rval)
{
	/* Write me! */
	return JS_TRUE;
}

const JSPropertySpec HTMLDocument_props[] = {
	{ "title",	JSP_HTML_DOCUMENT_TITLE,	JSPROP_ENUMERATE },
	{ "referer",	JSP_HTML_DOCUMENT_REFERER,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "domain",	JSP_HTML_DOCUMENT_DOMAIN,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "URL",	JSP_HTML_DOCUMENT_URL,		JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "body",	JSP_HTML_DOCUMENT_BODY,		JSPROP_ENUMERATE },
	{ "images",	JSP_HTML_DOCUMENT_IMAGES,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "applets",	JSP_HTML_DOCUMENT_APPLETS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "links",	JSP_HTML_DOCUMENT_LINKS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "forms",	JSP_HTML_DOCUMENT_FORMS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "anchors",	JSP_HTML_DOCUMENT_ANCHORS,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ "cookie",	JSP_HTML_DOCUMENT_COOKIE,	JSPROP_ENUMERATE },
	{ NULL }
};

const JSFunctionSpec HTMLDocument_funcs[] = {
	{ "open",		HTMLDocument_open,		0 },
	{ "close",		HTMLDocument_close,		0 },
	{ "write",		HTMLDocument_write,		1 },
	{ "writeln",		HTMLDocument_writeln,		1 },
	{ "getElementsByName",	HTMLDocument_getElementsByName,	1 },
	{ NULL }
};

const JSClass HTMLDocument_class = {
	"HTMLDocument",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	HTMLDocument_getProperty, HTMLDocument_setProperty,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Node_finalize
};

static struct dom_node *
get_document(struct dom_node *node)
{
	JSContext *ctx = node->ecmascript_ctx;
	struct html_objects *o;

	if (!ctx)
		return NULL;
	o = JS_GetContextPrivate(ctx);
	if (!o)
		return NULL;
	return o->document;
}

void
register_image(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;
		add_to_dom_node_list(&d->images, node, -1);
	}
}

void
unregister_image(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;

		del_from_dom_node_list(d->images, node);
	}
}

void
register_applet(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;
		add_to_dom_node_list(&d->applets, node, -1);
	}
}

void
unregister_applet(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;

		del_from_dom_node_list(d->applets, node);
	}
}

void
register_link(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;
		add_to_dom_node_list(&d->links, node, -1);
	}
}

void
unregister_link(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;

		del_from_dom_node_list(d->links, node);
	}
}

void
register_form(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;
		add_to_dom_node_list(&d->forms, node, -1);
	}
}

void
unregister_form(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;

		del_from_dom_node_list(d->forms, node);
	}
}

void
register_anchor(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;
		add_to_dom_node_list(&d->anchors, node, -1);
	}
}

void
unregister_anchor(struct dom_node *node)
{
	struct dom_node *doc = get_document(node);

	if (doc) {
		struct HTMLDocument_struct *d = doc->data.document.html_data;

		del_from_dom_node_list(d->anchors, node);
	}
}
