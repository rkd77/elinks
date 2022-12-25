/* The SpiderMonkey document object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/util.h"
#include <jsfriendapi.h>

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/css2xpath.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/collection.h"
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/forms.h"
#include "ecmascript/spidermonkey/implementation.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/element.h"
#include "ecmascript/spidermonkey/nodelist.h"
#include "ecmascript/spidermonkey/util.h"
#include "ecmascript/spidermonkey/window.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>

#include <iostream>

static xmlpp::Document emptyDoc;

static JSObject *getDoctype(JSContext *ctx, void *node);

static void document_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
}

JSClassOps document_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	document_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};


/* Each @document_class object must have a @window_class parent.  */
JSClass document_class = {
	"document",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&document_ops
};

static bool
document_get_property_anchors(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//a";
	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getCollection(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_get_property_baseURI(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	char *str = get_uri_string(vs->uri, URI_BASE);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	args.rval().setString(JS_NewStringCopyZ(ctx, str));
	mem_free(str);

	return true;
}

static bool
document_get_property_body(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//body";
	xmlpp::Node::NodeSet elements = root->find(xpath);

	if (elements.size() == 0) {
		args.rval().setNull();
		return true;
	}

	auto element = elements[0];

	JSObject *body = getElement(ctx, element);

	if (body) {
		args.rval().setObject(*body);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_set_property_body(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return true;
}

#ifdef CONFIG_COOKIES
static bool
document_get_property_cookie(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	struct view_state *vs;
	struct string *cookies;

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	cookies = send_cookies_js(vs->uri);

	if (cookies) {
		static char cookiestr[1024];

		strncpy(cookiestr, cookies->source, 1023);
		done_string(cookies);
		args.rval().setString(JS_NewStringCopyZ(ctx, cookiestr));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	}

	return true;
}

static bool
document_set_property_cookie(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	struct view_state *vs;

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *text = jsval_to_string(ctx, args[0]);
	set_cookie(vs->uri, text);
	mem_free_if(text);

	return true;
}

#endif

static bool
document_get_property_charset(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document* docu = (xmlpp::Document *)document->dom;
	xmlpp::ustring encoding = docu->get_encoding();

	if (encoding == "") {
		encoding = "utf-8";
	}

	args.rval().setString(JS_NewStringCopyZ(ctx, encoding.c_str()));

	return true;
}

static bool
document_get_property_childNodes(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct document *document = vs->doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	if (!root) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Node::NodeList *nodes = new xmlpp::Node::NodeList;

	*nodes = root->get_children();
	if (nodes->empty()) {
		delete nodes;
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getNodeList(ctx, nodes);
	args.rval().setObject(*elem);
	return true;
}


static bool
document_get_property_doctype(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document* docu = (xmlpp::Document *)document->dom;
	xmlpp::Dtd *dtd = docu->get_internal_subset();

	if (!dtd) {
		args.rval().setNull();
		return true;
	}
	JSObject *elem = getDoctype(ctx, dtd);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_get_property_documentElement(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//html";
	xmlpp::Node::NodeSet elements = root->find(xpath);

	if (elements.size() == 0) {
		args.rval().setNull();
		return true;
	}

	auto element = elements[0];

	JSObject *html = getElement(ctx, element);

	if (html) {
		args.rval().setObject(*html);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_get_property_documentURI(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	char *str = get_uri_string(vs->uri, URI_BASE);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	args.rval().setString(JS_NewStringCopyZ(ctx, str));
	mem_free(str);

	return true;
}

static bool
document_get_property_domain(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	char *str = get_uri_string(vs->uri, URI_HOST);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	args.rval().setString(JS_NewStringCopyZ(ctx, str));
	mem_free(str);

	return true;
}

static bool
document_get_property_forms(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//form";
	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getForms(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_get_property_head(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//head";
	xmlpp::Node::NodeSet elements = root->find(xpath);

	if (elements.size() == 0) {
		args.rval().setNull();
		return true;
	}

	auto element = elements[0];

	JSObject *head = getElement(ctx, element);

	if (head) {
		args.rval().setObject(*head);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_get_property_images(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//img";
	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getCollection(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_get_property_implementation(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	JSObject *obj = getImplementation(ctx);
	if (!obj) {
		args.rval().setNull();
	} else {
		args.rval().setObject(*obj);
	}
	return true;
}

static bool
document_get_property_links(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//a[@href]|//area[@href]";
	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getCollection(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_get_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, JS::GetNonCCWObjectGlobal(hobj));

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	args.rval().setObject(*(JSObject *)(interpreter->location_obj));
	return true;
}

static bool
document_get_property_nodeType(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setInt32(9);

	return true;
}


static bool
document_set_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	char *url = jsval_to_string(ctx, args[0]);

	if (url) {
		location_goto(doc_view, url);
		mem_free(url);
	}

	return true;
}


static bool
document_get_property_referrer(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;

	switch (get_opt_int("protocol.http.referer.policy", NULL)) {
	case REFERER_NONE:
		/* oh well */
		args.rval().setUndefined();
		break;

	case REFERER_FAKE:
		args.rval().setString(JS_NewStringCopyZ(ctx, get_opt_str("protocol.http.referer.fake", NULL)));
		break;

	case REFERER_TRUE:
		/* XXX: Encode as in add_url_to_httset_prop_string(&prop, ) ? --pasky */
		if (ses->referrer) {
			char *str = get_uri_string(ses->referrer, URI_HTTP_REFERRER);

			if (str) {
				args.rval().setString(JS_NewStringCopyZ(ctx, str));
				mem_free(str);
			} else {
				args.rval().setUndefined();
			}
		}
		break;

	case REFERER_SAME_URL:
		char *str = get_uri_string(document->uri, URI_HTTP_REFERRER);

		if (str) {
			args.rval().setString(JS_NewStringCopyZ(ctx, str));
			mem_free(str);
		} else {
			args.rval().setUndefined();
		}
		break;
	}

	return true;
}

static bool
document_get_property_scripts(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//script";
	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getCollection(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}


static bool
document_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	args.rval().setString(JS_NewStringCopyZ(ctx, document->title));

	return true;
}

static bool
document_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	JS::RootedObject parent_win(ctx, JS::GetNonCCWObjectGlobal(hobj));
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs || !vs->doc_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	mem_free_set(&document->title, jsval_to_string(ctx, args[0]));
	print_screen_status(doc_view->session);

	return true;
}

static bool
document_get_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	char *str = get_uri_string(document->uri, URI_ORIGINAL);

	if (str) {
		args.rval().setString(JS_NewStringCopyZ(ctx, str));
		mem_free(str);
	} else {
		args.rval().setUndefined();
	}

	return true;
}

static bool
document_set_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs;
	struct document_view *doc_view;

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	char *url = jsval_to_string(ctx, args[0]);
	if (url) {
		location_goto(doc_view, url);
		mem_free(url);
	}

	return true;
}


/* "cookie" is special; it isn't a regular property but we channel it to the
 * cookie-module. XXX: Would it work if "cookie" was defined in this array? */
JSPropertySpec document_props[] = {
	JS_PSG("anchors", document_get_property_anchors, JSPROP_ENUMERATE),
	JS_PSG("baseURI", document_get_property_baseURI, JSPROP_ENUMERATE),
	JS_PSGS("body", document_get_property_body, document_set_property_body, JSPROP_ENUMERATE),
#ifdef CONFIG_COOKIES
	JS_PSGS("cookie", document_get_property_cookie, document_set_property_cookie, JSPROP_ENUMERATE),
#endif

	JS_PSG("charset", document_get_property_charset, JSPROP_ENUMERATE),
	JS_PSG("characterSet", document_get_property_charset, JSPROP_ENUMERATE),
	JS_PSG("childNodes", document_get_property_childNodes, JSPROP_ENUMERATE),
	JS_PSG("doctype", document_get_property_doctype, JSPROP_ENUMERATE),
	JS_PSG("documentElement", document_get_property_documentElement, JSPROP_ENUMERATE),
	JS_PSG("documentURI", document_get_property_documentURI, JSPROP_ENUMERATE),
	JS_PSG("domain", document_get_property_domain, JSPROP_ENUMERATE),
	JS_PSG("forms", document_get_property_forms, JSPROP_ENUMERATE),
	JS_PSG("head", document_get_property_head, JSPROP_ENUMERATE),
	JS_PSG("images", document_get_property_images, JSPROP_ENUMERATE),
	JS_PSG("implementation", document_get_property_implementation, JSPROP_ENUMERATE),
	JS_PSG("inputEncoding", document_get_property_charset, JSPROP_ENUMERATE),
	JS_PSG("links", document_get_property_links, JSPROP_ENUMERATE),
	JS_PSGS("location",	document_get_property_location, document_set_property_location, JSPROP_ENUMERATE),
	JS_PSG("nodeType", document_get_property_nodeType, JSPROP_ENUMERATE),
	JS_PSG("referrer",	document_get_property_referrer, JSPROP_ENUMERATE),
	JS_PSG("scripts",	document_get_property_scripts, JSPROP_ENUMERATE),
	JS_PSGS("title",	document_get_property_title, document_set_property_title, JSPROP_ENUMERATE), /* TODO: Charset? */
	JS_PSGS("URL",	document_get_property_url, document_set_property_url, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool document_createComment(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_createDocumentFragment(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_createElement(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_createTextNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_write(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_writeln(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_replace(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementById(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementsByClassName(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementsByName(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementsByTagName(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_querySelector(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_querySelectorAll(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec document_funcs[] = {
	{ "createComment",	document_createComment, 1 },
	{ "createDocumentFragment",	document_createDocumentFragment, 0 },
	{ "createElement",	document_createElement, 1 },
	{ "createTextNode",	document_createTextNode, 1 },
	{ "write",		document_write,		1 },
	{ "writeln",		document_writeln,	1 },
	{ "replace",		document_replace,	2 },
	{ "getElementById",	document_getElementById,	1 },
	{ "getElementsByClassName",	document_getElementsByClassName,	1 },
	{ "getElementsByName",	document_getElementsByName,	1 },
	{ "getElementsByTagName",	document_getElementsByTagName,	1 },
	{ "querySelector",	document_querySelector,	1 },
	{ "querySelectorAll",	document_querySelectorAll,	1 },
	{ NULL }
};


static bool
document_write_do(JSContext *ctx, unsigned int argc, JS::Value *rval, int newline)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

	if (argc >= 1)
	{
		interpreter->write_element_offset = interpreter->element_offset;
		for (unsigned int i = 0; i < argc; ++i)
		{
			char *str = jsval_to_string(ctx, args[i]);

			if (str) {
				add_to_string(&interpreter->writecode, str);
				mem_free(str);
			}
		}

		if (newline)
		{
			add_to_string(&interpreter->writecode, "\n");
		}
	}
	interpreter->changed = true;

#ifdef CONFIG_LEDS
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif

	args.rval().setBoolean(false);

	return true;
}

/* @document_funcs{"write"} */
static bool
document_write(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	return document_write_do(ctx, argc, rval, 0);
}

/* @document_funcs{"writeln"} */
static bool
document_writeln(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return document_write_do(ctx, argc, rval, 1);
}

/* @document_funcs{"replace"} */
static bool
document_replace(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 2) {
		args.rval().setBoolean(false);
		return(true);
	}

	struct string needle;
	struct string heystack;

	if (!init_string(&needle)) {
		return false;
	}
	if (!init_string(&heystack)) {
		done_string(&needle);
		return false;
	}

	jshandle_value_to_char_string(&needle, ctx, args[0]);
	jshandle_value_to_char_string(&heystack, ctx, args[1]);

	//DBG("doc replace %s %s\n", needle.source, heystack.source);

	struct document *document = doc_view->document;
	struct cache_entry *cached = document->cached;
	struct fragment *f = get_cache_fragment(cached);

	if (f && f->length)
	{
		struct string f_data;
		if (init_string(&f_data)) {
			add_bytes_to_string(&f_data, f->data, f->length);

			struct string nu_str;
			if (init_string(&nu_str)) {
				string_replace(&nu_str, &f_data, &needle, &heystack);
				delete_entry_content(cached);
				/* TBD: somehow better rerender the document 
				 * now it's places on the session level in doc_loading_callback */
				add_fragment(cached, 0, nu_str.source, nu_str.length);
				normalize_cache_entry(cached, nu_str.length);
				document->ecmascript_counter++;
				done_string(&nu_str);
			}
				//DBG("doc replace %s %s\n", needle.source, heystack.source);
			done_string(&f_data);
		}
	}

	done_string(&needle);
	done_string(&heystack);

	args.rval().setBoolean(true);

	return(true);
}

static bool
document_createComment(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	xmlpp::Element* emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		emptyDoc.create_root_node("root");
	}

	emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		args.rval().setNull();
		return true;
	}

	struct string idstr;
	if (!init_string(&idstr)) {
		return false;
	}
	jshandle_value_to_char_string(&idstr, ctx, args[0]);
	xmlpp::ustring text = idstr.source;
	done_string(&idstr);

	xmlpp::CommentNode *comment = emptyRoot->add_child_comment(text);

	if (!comment) {
		args.rval().setNull();
		return true;
	}

	JSObject *jelem = getElement(ctx, comment);
	args.rval().setObject(*jelem);

	return true;
}

static bool
document_createDocumentFragment(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 0) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *doc2 = static_cast<xmlpp::Document *>(document->dom);
	xmlDoc *docu = doc2->cobj();
	xmlNode *xmlnode = xmlNewDocFragment(docu);

	if (!xmlnode) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Node *node = new xmlpp::Node(xmlnode);

	JSObject *jelem = getElement(ctx, node);
	args.rval().setObject(*jelem);

	return true;
}


static bool
document_createElement(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	xmlpp::Element* emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		emptyDoc.create_root_node("root");
	}

	emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		args.rval().setNull();
		return true;
	}

	struct string idstr;
	if (!init_string(&idstr)) {
		return false;
	}
	jshandle_value_to_char_string(&idstr, ctx, args[0]);
	xmlpp::ustring text = idstr.source;
	done_string(&idstr);

	xmlpp::Element *elem = emptyRoot->add_child_element(text);

	if (!elem) {
		args.rval().setNull();
		return true;
	}

	JSObject *jelem = getElement(ctx, elem);
	args.rval().setObject(*jelem);

	return true;
}

static bool
document_createTextNode(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	xmlpp::Element* emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		emptyDoc.create_root_node("root");
	}

	emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		args.rval().setNull();
		return true;
	}

	struct string idstr;
	if (!init_string(&idstr)) {
		return false;
	}
	jshandle_value_to_char_string(&idstr, ctx, args[0]);
	xmlpp::ustring text = idstr.source;
	done_string(&idstr);

	xmlpp::TextNode *textNode = emptyRoot->add_child_text(text);

	if (!textNode) {
		args.rval().setNull();
		return true;
	}

	JSObject *jelem = getElement(ctx, textNode);
	args.rval().setObject(*jelem);

	return true;
}

static bool
document_getElementById(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	struct string idstr;

	if (!init_string(&idstr)) {
		return false;
	}
	jshandle_value_to_char_string(&idstr, ctx, args[0]);
	xmlpp::ustring id = idstr.source;

	xmlpp::ustring xpath = "//*[@id=\"";
	xpath += id;
	xpath += "\"]";

	done_string(&idstr);

	auto elements = root->find(xpath);

	if (elements.size() == 0) {
		args.rval().setNull();
		return true;
	}

	auto node = elements[0];

	JSObject *elem = getElement(ctx, node);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_getElementsByClassName(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	struct string idstr;

	if (!init_string(&idstr)) {
		return false;
	}
	jshandle_value_to_char_string(&idstr, ctx, args[0]);
	xmlpp::ustring id = idstr.source;

	xmlpp::ustring xpath = "//*[@class=\"";
	xpath += id;
	xpath += "\"]";

	done_string(&idstr);

	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	*elements = root->find(xpath);

	JSObject *elem = getCollection(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_getElementsByName(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	struct string idstr;

	if (!init_string(&idstr)) {
		return false;
	}
	jshandle_value_to_char_string(&idstr, ctx, args[0]);
	xmlpp::ustring id = idstr.source;

	xmlpp::ustring xpath = "//*[@id=\"";
	xpath += id;
	xpath += "\"]|//*[@name=\"";
	xpath += id;
	xpath += "\"]";

	done_string(&idstr);

	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	*elements = root->find(xpath);

	JSObject *elem = getCollection(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_getElementsByTagName(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	struct string idstr;

	if (!init_string(&idstr)) {
		return false;
	}
	jshandle_value_to_char_string(&idstr, ctx, args[0]);
	xmlpp::ustring id = idstr.source;
	std::transform(id.begin(), id.end(), id.begin(), ::tolower);

	xmlpp::ustring xpath = "//";
	xpath += id;

	done_string(&idstr);

	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	*elements = root->find(xpath);

	JSObject *elem = getCollection(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_querySelector(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	struct string cssstr;

	if (!init_string(&cssstr)) {
		return false;
	}
	jshandle_value_to_char_string(&cssstr, ctx, args[0]);
	xmlpp::ustring css = cssstr.source;

	xmlpp::ustring xpath = css2xpath(css);

	done_string(&cssstr);

	xmlpp::Node::NodeSet elements;

	try {
		elements = root->find(xpath);
	} catch (xmlpp::exception &e) {
		args.rval().setNull();
		return true;
	}

	if (elements.size() == 0) {
		args.rval().setNull();
		return true;
	}

	auto node = elements[0];

	JSObject *elem = getElement(ctx, node);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

static bool
document_querySelectorAll(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	struct string cssstr;

	if (!init_string(&cssstr)) {
		return false;
	}
	jshandle_value_to_char_string(&cssstr, ctx, args[0]);
	xmlpp::ustring css = cssstr.source;

	xmlpp::ustring xpath = css2xpath(css);

	done_string(&cssstr);

	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	try {
		*elements = root->find(xpath);
	} catch (xmlpp::exception &e) {
	}

	JSObject *elem = getCollection(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

	return true;
}

JSClassOps doctype_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass doctype_class = {
	"doctype",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&doctype_ops
};

static bool
doctype_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &doctype_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Dtd *dtd = JS::GetMaybePtrFromReservedSlot<xmlpp::Dtd>(hobj, 0);

	if (!dtd) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = dtd->get_name();
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static bool
doctype_get_property_publicId(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &doctype_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Dtd *dtd = JS::GetMaybePtrFromReservedSlot<xmlpp::Dtd>(hobj, 0);

	if (!dtd) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = dtd->get_external_id();
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static bool
doctype_get_property_systemId(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &doctype_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Dtd *dtd = JS::GetMaybePtrFromReservedSlot<xmlpp::Dtd>(hobj, 0);

	if (!dtd) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = dtd->get_system_id();
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

JSPropertySpec doctype_props[] = {
	JS_PSG("name", doctype_get_property_name, JSPROP_ENUMERATE),
	JS_PSG("publicId", doctype_get_property_publicId, JSPROP_ENUMERATE),
	JS_PSG("systemId", doctype_get_property_systemId, JSPROP_ENUMERATE),
	JS_PS_END
};

static JSObject *
getDoctype(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *el = JS_NewObject(ctx, &doctype_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);
	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) doctype_props);
	JS::SetReservedSlot(el, 0, JS::PrivateValue(node));

	return el;
}

JSObject *
getDocument(JSContext *ctx, void *doc)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *el = JS_NewObject(ctx, &document_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) document_props);
	spidermonkey_DefineFunctions(ctx, el, document_funcs);

	JS::SetReservedSlot(el, 0, JS::PrivateValue(doc));

	return el;
}
