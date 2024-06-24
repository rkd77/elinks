/* The SpiderMonkey document object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/libdom/dom.h"

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
#include "document/libdom/doc.h"
#include "document/libdom/renderer2.h"
#include "document/view.h"
#include "ecmascript/css2xpath.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/libdom/parse.h"
#include "ecmascript/spidermonkey/collection.h"
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/forms.h"
#include "ecmascript/spidermonkey/implementation.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/element.h"
#include "ecmascript/spidermonkey/event.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/nodelist.h"
#include "ecmascript/spidermonkey/nodelist2.h"
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

#include <iostream>
#include <map>

static std::map<void *, bool> handler_privates;

struct el_listener {
	LIST_HEAD_EL(struct el_listener);
	char *typ;
	JS::Heap<JS::Value> *fun;
};

enum readyState {
	LOADING = 0,
	INTERACTIVE,
	COMPLETE
};

struct document_private {
	LIST_OF(struct el_listener) listeners;
	struct ecmascript_interpreter *interpreter;
	dom_document *doc;
	JS::Heap<JSObject *> thisval;
	dom_event_listener *listener;
	int ref_count;
	enum readyState state;
};

static JSObject *getDoctype(JSContext *ctx, void *node);
static void document_event_handler(dom_event *event, void *pw);

static void document_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(obj, 0);
	struct document_private *doc_private = JS::GetMaybePtrFromReservedSlot<struct document_private>(obj, 1);

	if (doc_private) {
		auto h = handler_privates.find(doc_private);

		if (h != handler_privates.end()) {
			handler_privates.erase(h);
		}

		struct el_listener *l;

		if (doc_private->listener) {
			dom_event_listener_unref(doc_private->listener);
		}

		foreach(l, doc_private->listeners) {
			mem_free_set(&l->typ, NULL);
			delete (l->fun);
		}
		free_list(doc_private->listeners);
		mem_free(doc_private);
		JS::SetReservedSlot(obj, 1, JS::UndefinedValue());
	}

	if (doc) {
		dom_node_unref(doc);
		JS::SetReservedSlot(obj, 0, JS::UndefinedValue());
	}
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
	JSCLASS_HAS_RESERVED_SLOTS(2),
	&document_ops
};

static bool
document_get_property_anchors(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_html_document *doc = (dom_html_document *)JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_html_collection *anchors = NULL;
	dom_exception exc = dom_html_document_get_anchors(doc, &anchors);

	if (exc != DOM_NO_ERR || !anchors) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getCollection(ctx, anchors);
	args.rval().setObject(*obj);

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
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_html_document *doc = (dom_html_document *)JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_html_element *body = NULL;
	dom_exception exc = dom_html_document_get_body(doc, &body);

	if (exc != DOM_NO_ERR || !body) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getElement(ctx, body);
	args.rval().setObject(*obj);

	return true;
}

static bool
document_set_property_body(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	// TODO
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
// TODO
	args.rval().setString(JS_NewStringCopyZ(ctx, "utf-8"));

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
	dom_html_document *doc = (dom_html_document *)JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_element *root = NULL;
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR || !root) {
		args.rval().setNull();
		return true;
	}
	dom_nodelist *nodes = NULL;
	exc = dom_node_get_child_nodes(root, &nodes);
	dom_node_unref(root);

	if (exc != DOM_NO_ERR || !nodes) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getNodeList(ctx, nodes);
	args.rval().setObject(*obj);

	return true;
}

static bool
document_get_property_defaultView(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	JS::RootedObject win(ctx);
	JS::Heap<JSObject*> *window_obj = interpreter->ac;
	win = window_obj->get();
	args.rval().setObject(*win);

	return true;
}

static bool
document_get_property_doctype(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_html_document *doc = (dom_html_document *)JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_document_type *dtd;
	dom_document_get_doctype(doc, &dtd);

	JSObject *obj = getDoctype(ctx, dtd);
	args.rval().setObject(*obj);

	return true;
}

static bool
document_get_property_documentElement(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_html_document *doc = (dom_html_document *)JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_html_element *root = NULL;
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR || !root) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getElement(ctx, root);
	args.rval().setObject(*obj);

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
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_html_document *doc = (dom_html_document *)JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_html_collection *forms = NULL;
	dom_exception exc = dom_html_document_get_forms(doc, &forms);

	if (exc != DOM_NO_ERR || !forms) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getForms(ctx, forms);
	args.rval().setObject(*obj);

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
// TODO
	args.rval().setNull();
	return true;
}

static bool
document_get_property_images(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_html_document *doc = (dom_html_document *)JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_html_collection *images = NULL;
	dom_exception exc = dom_html_document_get_images(doc, &images);

	if (exc != DOM_NO_ERR || !images) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getCollection(ctx, images);
	args.rval().setObject(*obj);

	return true;
}

static bool
document_get_property_implementation(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

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
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_html_document *doc = (dom_html_document *)JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_html_collection *links = NULL;
	dom_exception exc = dom_html_document_get_links(doc, &links);

	if (exc != DOM_NO_ERR || !links) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getCollection(ctx, links);
	args.rval().setObject(*obj);

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

	if (!interpreter->location_obj) {
		interpreter->location_obj = getLocation(ctx);
	}
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
document_get_property_readyState(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct document_private *doc_private = JS::GetMaybePtrFromReservedSlot<struct document_private>(hobj, 1);

	if (!doc_private) {
		args.rval().setNull();
		return true;
	}

	switch (doc_private->state) {
	case LOADING:
		args.rval().setString(JS_NewStringCopyZ(ctx, "loading"));
		break;
	case INTERACTIVE:
		args.rval().setString(JS_NewStringCopyZ(ctx, "interactive"));
		break;
	case COMPLETE:
		args.rval().setString(JS_NewStringCopyZ(ctx, "complete"));
		break;
	}

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
	char *str;

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
			str = get_uri_string(ses->referrer, URI_HTTP_REFERRER);

			if (str) {
				args.rval().setString(JS_NewStringCopyZ(ctx, str));
				mem_free(str);
			} else {
				args.rval().setUndefined();
			}
		}
		break;

	case REFERER_SAME_URL:
		str = get_uri_string(document->uri, URI_HTTP_REFERRER);

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
// TODO
	args.rval().setNull();

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
	JS_PSG("defaultView", document_get_property_defaultView, JSPROP_ENUMERATE),
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
	JS_PSG("readyState", document_get_property_readyState, JSPROP_ENUMERATE),
	JS_PSG("referrer",	document_get_property_referrer, JSPROP_ENUMERATE),
	JS_PSG("scripts",	document_get_property_scripts, JSPROP_ENUMERATE),
	JS_PSGS("title",	document_get_property_title, document_set_property_title, JSPROP_ENUMERATE), /* TODO: Charset? */
	JS_PSGS("URL",	document_get_property_url, document_set_property_url, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool document_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_createComment(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_createDocumentFragment(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_createElement(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_createTextNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_dispatchEvent(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_write(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_writeln(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_replace(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementById(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementsByClassName(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementsByName(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementsByTagName(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_querySelector(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_querySelectorAll(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec document_funcs[] = {
	{ "addEventListener",	document_addEventListener,	3 },
	{ "createComment",	document_createComment, 1 },
	{ "createDocumentFragment",	document_createDocumentFragment, 0 },
	{ "createElement",	document_createElement, 1 },
	{ "createTextNode",	document_createTextNode, 1 },
	{ "dispatchEvent",	document_dispatchEvent, 1 },
	{ "write",		document_write,		1 },
	{ "writeln",		document_writeln,	1 },
	{ "replace",		document_replace,	2 },
	{ "getElementById",	document_getElementById,	1 },
//	{ "getElementsByClassName",	document_getElementsByClassName,	1 },
//	{ "getElementsByName",	document_getElementsByName,	1 },
	{ "getElementsByTagName",	document_getElementsByTagName,	1 },
	{ "querySelector",	document_querySelector,	1 },
	{ "querySelectorAll",	document_querySelectorAll,	1 },
	{ "removeEventListener",	document_removeEventListener,	2 },
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
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

	if (argc >= 1) {
		int element_offset = interpreter->element_offset;

		struct string string;

		if (init_string(&string)) {
			for (unsigned int i = 0; i < argc; ++i) {
				char *str = jsval_to_string(ctx, args[i]);

				if (str) {
					add_to_string(&string, str);
					mem_free(str);
				}
			}

			if (newline) {
				add_to_string(&string, "\n");
			}
			if (element_offset == interpreter->current_writecode->element_offset) {
				add_string_to_string(&interpreter->current_writecode->string, &string);
				done_string(&string);
			} else {
				(void)add_to_ecmascript_string_list(&interpreter->writecode, string.source, string.length, element_offset);
				done_string(&string);
				interpreter->current_writecode = interpreter->current_writecode->next;
			}
			interpreter->changed = 1;
			interpreter->was_write = 1;
			interpreter->onload_snippets_cache_id = 1;
			debug_dump_xhtml(document->dom);
		}
	}

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
				el_string_replace(&nu_str, &f_data, &needle, &heystack);
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
document_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);
	struct document_private *doc_private = JS::GetMaybePtrFromReservedSlot<struct document_private>(hobj, 1);

	if (!doc || !doc_private) {
		args.rval().setNull();
		return true;
	}

	if (argc < 2) {
		args.rval().setUndefined();
		return true;
	}
	char *method = jsval_to_string(ctx, args[0]);

	if (!method) {
		args.rval().setUndefined();
		return true;
	}
	JS::RootedValue fun(ctx, args[1]);
	struct el_listener *l;

	foreach(l, doc_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (*(l->fun) == fun) {
			mem_free(method);
			args.rval().setUndefined();
			return true;
		}
	}
	struct el_listener *n = (struct el_listener *)mem_calloc(1, sizeof(*n));

	if (!n) {
		args.rval().setUndefined();
		return false;
	}
	n->fun = new JS::Heap<JS::Value>(fun);
	n->typ = method;
	add_to_list_end(doc_private->listeners, n);
	dom_exception exc;

	if (doc_private->listener) {
		dom_event_listener_ref(doc_private->listener);
	} else {
		exc = dom_event_listener_create(document_event_handler, doc_private, &doc_private->listener);

		if (exc != DOM_NO_ERR || !doc_private->listener) {
			args.rval().setUndefined();
			return true;
		}
		handler_privates[doc_private] = true;
	}
	dom_string *typ = NULL;
	exc = dom_string_create(method, strlen(method), &typ);

	if (exc != DOM_NO_ERR || !typ) {
		goto ex;
	}
	exc = dom_event_target_add_event_listener(doc, typ, doc_private->listener, false);

	if (exc == DOM_NO_ERR) {
		dom_event_listener_ref(doc_private->listener);
	}

ex:
	if (typ) {
		dom_string_unref(typ);
	}
	dom_event_listener_unref(doc_private->listener);
	args.rval().setUndefined();

	return true;
}

static bool
document_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);
	struct document_private *doc_private = JS::GetMaybePtrFromReservedSlot<struct document_private>(hobj, 1);

	if (!doc || !doc_private) {
		args.rval().setNull();
		return true;
	}

	if (argc < 2) {
		args.rval().setUndefined();
		return true;
	}
	char *method = jsval_to_string(ctx, args[0]);

	if (!method) {
		return false;
	}
	JS::RootedValue fun(ctx, args[1]);
	struct  el_listener *l;

	foreach(l, doc_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (*(l->fun) == fun) {

			dom_string *typ = NULL;
			dom_exception exc = dom_string_create(method, strlen(method), &typ);

			if (exc != DOM_NO_ERR || !typ) {
				continue;
			}
			//dom_event_target_remove_event_listener(doc, typ, doc_private->listener, false);
			dom_string_unref(typ);

			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			delete (l->fun);
			mem_free(l);
			mem_free(method);
			args.rval().setUndefined();
			return true;
		}
	}
	mem_free(method);
	args.rval().setUndefined();
	return true;
}

static bool
document_createComment(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_string *data = NULL;
	dom_exception exc;
	char *str;
	size_t len;

	str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	len = strlen(str);
	exc = dom_string_create((const uint8_t *)str, len, &data);
	mem_free(str);

	if (exc != DOM_NO_ERR || !data) {
		args.rval().setNull();
		return true;
	}
	dom_comment *comment = NULL;
	exc = dom_document_create_comment(doc, data, &comment);
	dom_string_unref(data);

	if (exc != DOM_NO_ERR || !comment) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getElement(ctx, comment);
	args.rval().setObject(*obj);

	return true;
}

static bool
document_createDocumentFragment(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc != 0) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_document_fragment *fragment = NULL;
	dom_exception exc = dom_document_create_document_fragment(doc, &fragment);

	if (exc != DOM_NO_ERR || !fragment) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getElement(ctx, fragment);
	args.rval().setObject(*obj);

	return true;
}


static bool
document_createElement(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);
	dom_string *tag_name = NULL;
	dom_exception exc;
	char *str;
	size_t len;

	str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	len = strlen(str);
	exc = dom_string_create((const uint8_t *)str, len, &tag_name);
	mem_free(str);

	if (exc != DOM_NO_ERR || !tag_name) {
		args.rval().setNull();
		return true;
	}
	dom_element *element = NULL;
	exc = dom_document_create_element(doc, tag_name, &element);
	dom_string_unref(tag_name);

	if (exc != DOM_NO_ERR || !element) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getElement(ctx, element);
	args.rval().setObject(*obj);

	return true;
}

static bool
document_dispatchEvent(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);
	struct document_private *doc_private = JS::GetMaybePtrFromReservedSlot<struct document_private>(hobj, 1);

	if (!doc || !doc_private) {
		args.rval().setBoolean(false);
		return true;
	}

	if (argc < 1) {
		args.rval().setBoolean(false);
		return true;
	}
	JS::RootedObject eve(ctx, &args[0].toObject());
	dom_event *event = (dom_event *)JS::GetMaybePtrFromReservedSlot<dom_event>(eve, 0);
	bool result = false;
	dom_exception exc = dom_event_target_dispatch_event(doc, event, &result);

	args.rval().setBoolean(result);
	return true;
}

static void
document_event_handler(dom_event *event, void *pw)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct document_private *doc_private = (struct document_private *)pw;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)doc_private->interpreter;
	JSContext *ctx = (JSContext *)interpreter->backend_data;
	dom_document *doc = (dom_document *)doc_private->doc;

	JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());
	JS::RootedValue r_val(ctx);

	if (!event) {
		return;
	}
	auto h = handler_privates.find(doc_private);

	if (h == handler_privates.end()) {
		return;
	}
	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		return;
	}

	if (!strcmp("DOMContentLoaded", dom_string_data(typ))) {
		if (doc_private->state == COMPLETE) {
			dom_string_unref(typ);
			return;
		}
		doc_private->state = COMPLETE;
	}
	JSObject *obj_ev = getEvent(ctx, event);
	interpreter->heartbeat = add_heartbeat(interpreter);
	struct el_listener *l, *next;

	foreachsafe(l, next, doc_private->listeners) {
		if (strcmp(l->typ, dom_string_data(typ))) {
			continue;
		}
		JS::RootedValueArray<1> argv(ctx);
		argv[0].setObject(*obj_ev);
		JS::RootedValue r_val(ctx);
		JS::RootedObject thisv(ctx, doc_private->thisval);
		JS::RootedValue vfun(ctx, *(l->fun));
		JS_CallFunctionValue(ctx, thisv, vfun, argv, &r_val);
	}
	done_heartbeat(interpreter->heartbeat);
	check_for_rerender(interpreter, dom_string_data(typ));
	dom_string_unref(typ);
}

static bool
document_createTextNode(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);
	dom_string *data = NULL;
	dom_exception exc;
	char *str;
	size_t len;

	str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	len = strlen(str);
	exc = dom_string_create((const uint8_t *)str, len, &data);
	mem_free(str);

	if (exc != DOM_NO_ERR || !data) {
		args.rval().setNull();
		return true;
	}
	dom_text *text_node = NULL;
	exc = dom_document_create_text_node(doc, data, &text_node);
	dom_string_unref(data);

	if (exc != DOM_NO_ERR || !text_node) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getElement(ctx, text_node);
	args.rval().setObject(*obj);

	return true;
}

static bool
document_getElementById(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_string *id = NULL;
	dom_exception exc;
	char *str;
	size_t len;

	str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	len = strlen(str);
	exc = dom_string_create((const uint8_t *)str, len, &id);
	mem_free(str);

	if (exc != DOM_NO_ERR || !id) {
		args.rval().setNull();
		return true;
	}
	dom_element *element = NULL;
	exc = dom_document_get_element_by_id(doc, id, &element);
	dom_string_unref(id);

	if (exc != DOM_NO_ERR || !element) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getElement(ctx, element);
	args.rval().setObject(*obj);

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

// TODO
	args.rval().setNull();

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

// TODO
	args.rval().setNull();

	return true;
}

static bool
document_getElementsByTagName(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_string *tagname = NULL;
	dom_exception exc;
	char *str;
	size_t len;

	str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	len = strlen(str);
	exc = dom_string_create((const uint8_t *)str, len, &tagname);
	mem_free(str);

	if (exc != DOM_NO_ERR || !tagname) {
		args.rval().setNull();
		return true;
	}
	dom_nodelist *nodes = NULL;
	exc = dom_document_get_elements_by_tag_name(doc, tagname, &nodes);
	dom_string_unref(tagname);

	if (exc != DOM_NO_ERR || !nodes) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getNodeList(ctx, nodes);
	args.rval().setObject(*obj);

	return true;
}

static bool
document_querySelector(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_node *root = NULL; /* root element of document */
	/* Get root element */
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	char *selector = jsval_to_string(ctx, args[0]);

	if (!selector) {
		dom_node_unref(root);
		args.rval().setNull();
		return true;
	}
	void *ret = walk_tree_query(root, selector, 0);
	mem_free(selector);
	dom_node_unref(root);

	if (!ret) {
		args.rval().setNull();
	} else {
		JSObject *el = getElement(ctx, ret);
		args.rval().setObject(*el);
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
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_document *doc = JS::GetMaybePtrFromReservedSlot<dom_document>(hobj, 0);

	if (!doc) {
		args.rval().setNull();
		return true;
	}
	dom_node *doc_root = NULL; /* root element of document */
	/* Get root element */
	dom_exception exc = dom_document_get_document_element(doc, &doc_root);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	char *selector = jsval_to_string(ctx, args[0]);

	if (!selector) {
		dom_node_unref(doc_root);
		args.rval().setNull();
		return true;
	}

	dom_string *tag_name = NULL;
	exc = dom_string_create((const uint8_t *)"B", 1, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		dom_node_unref(doc_root);
		mem_free(selector);
		args.rval().setNull();
		return true;
	}
	dom_element *element = NULL;
	exc = dom_document_create_element(doc, tag_name, &element);
	dom_string_unref(tag_name);

	if (exc != DOM_NO_ERR || !element) {
		dom_node_unref(doc_root);
		mem_free(selector);
		args.rval().setNull();
		return true;
	}
	LIST_OF(struct selector_node) *result_list = (LIST_OF(struct selector_node) *)mem_calloc(1, sizeof(*result_list));

	if (!result_list) {
		dom_node_unref(doc_root);
		mem_free(selector);
		args.rval().setNull();
		return true;
	}
	init_list(*result_list);

	walk_tree_query_append((dom_node *)element, doc_root, selector, 0, result_list);
	dom_node_unref(doc_root);
	mem_free(selector);

	JSObject *obj = getNodeList2(ctx, result_list);
	args.rval().setObject(*obj);

	return true;
}

static void doctype_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_document_type *dtd = JS::GetMaybePtrFromReservedSlot<dom_document_type>(obj, 0);

	if (dtd) {
		dom_node_unref(dtd);
	}
}

JSClassOps doctype_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	doctype_finalize,  // finalize
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
	dom_document_type *dtd = (dom_document_type *)JS::GetMaybePtrFromReservedSlot<dom_document_type>(hobj, 0);

	if (!dtd) {
		args.rval().setNull();
		return true;
	}
	dom_string *name = NULL;
	dom_exception exc = dom_document_type_get_name(dtd, &name);

	if (exc != DOM_NO_ERR || !name) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(name)));
	dom_string_unref(name);

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
	dom_document_type *dtd = (dom_document_type *)JS::GetMaybePtrFromReservedSlot<dom_document_type>(hobj, 0);

	if (!dtd) {
		args.rval().setNull();
		return true;
	}
	dom_string *public_id = NULL;
	dom_exception exc = dom_document_type_get_public_id(dtd, &public_id);

	if (exc != DOM_NO_ERR || !public_id) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(public_id)));
	dom_string_unref(public_id);

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
	dom_document_type *dtd = (dom_document_type *)JS::GetMaybePtrFromReservedSlot<dom_document_type>(hobj, 0);

	if (!dtd) {
		args.rval().setNull();
		return true;
	}
	dom_string *system_id = NULL;
	dom_exception exc = dom_document_type_get_system_id(dtd, &system_id);

	if (exc != DOM_NO_ERR || !system_id) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(system_id)));
	dom_string_unref(system_id);

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
	struct document_private *doc_private = (struct document_private *)mem_calloc(1, sizeof(*doc_private));

	if (!doc_private) {
		return NULL;
	}
	init_list(doc_private->listeners);
	doc_private->ref_count = 1;
	doc_private->doc = doc;
	JSObject *el = JS_NewObject(ctx, &document_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	doc_private->interpreter = interpreter;
	doc_private->thisval = r_el;

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) document_props);
	spidermonkey_DefineFunctions(ctx, el, document_funcs);

	JS::SetReservedSlot(el, 0, JS::PrivateValue(doc));
	JS::SetReservedSlot(el, 1, JS::PrivateValue(doc_private));

	return el;
}

bool
initDocument(JSContext *ctx, struct ecmascript_interpreter *interpreter, JSObject *document_obj, void *doc)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct document_private *doc_private = (struct document_private *)mem_calloc(1, sizeof(*doc_private));

	if (!doc_private) {
		return false;
	}
	JS::RootedObject r_el(ctx, document_obj);
	doc_private->interpreter = interpreter;
	doc_private->thisval = r_el;
	doc_private->doc = doc;

	init_list(doc_private->listeners);
	doc_private->ref_count = 1;
	JS::SetReservedSlot(document_obj, 0, JS::PrivateValue(doc));
	JS::SetReservedSlot(document_obj, 1, JS::PrivateValue(doc_private));

	return true;
}
