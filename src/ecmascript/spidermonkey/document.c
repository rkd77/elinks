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
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/element.h"
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

static bool document_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

JSClassOps document_ops = {
	JS_PropertyStub, nullptr,
	document_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr
};


/* Each @document_class object must have a @window_class parent.  */
JSClass document_class = {
	"document",
	JSCLASS_HAS_PRIVATE,
	&document_ops
};

static bool
document_get_property_anchors(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	std::string xpath = "//a";
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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	char *str = get_uri_string(vs->uri, URI_BASE);

	if (!str) {
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	std::string xpath = "//body";
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

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct string *cookies;

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	cookies = send_cookies_js(vs->uri);

	if (cookies) {
		static unsigned char cookiestr[1024];

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

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct string *cookies;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	set_cookie(vs->uri, JS_EncodeString(ctx, args[0].toString()));

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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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
	std::string encoding = docu->get_encoding();

	if (encoding == "") {
		encoding = "utf-8";
	}

	args.rval().setString(JS_NewStringCopyZ(ctx, encoding.c_str()));

	return true;
}

static bool
document_get_property_doctype(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	std::string xpath = "//html";
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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	char *str = get_uri_string(vs->uri, URI_BASE);

	if (!str) {
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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &document_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	char *str = get_uri_string(vs->uri, URI_HOST);

	if (!str) {
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	std::string xpath = "//form";
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	std::string xpath = "//head";
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	std::string xpath = "//img";
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
document_get_property_links(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	std::string xpath = "//a[@href]|//area[@href]";
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
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

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

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
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

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	location_goto(doc_view, JS_EncodeString(ctx, args[0].toString()));

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

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;

	vs = interpreter->vs;

	if (!vs) {
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	std::string xpath = "//script";
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

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	args.rval().setString(JS_NewStringCopyZ(ctx, document->title));

	return true;
}

static bool
document_set_property_title(JSContext *ctx, int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);


	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs || !vs->doc_view) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	mem_free_set(&document->title, stracpy(JS_EncodeString(ctx, args[0].toString())));
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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs) {
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
document_set_property_url(JSContext *ctx, int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	location_goto(doc_view, JS_EncodeString(ctx, args[0].toString()));

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
	JS_PSG("doctype", document_get_property_doctype, JSPROP_ENUMERATE),
	JS_PSG("documentElement", document_get_property_documentElement, JSPROP_ENUMERATE),
	JS_PSG("documentURI", document_get_property_documentURI, JSPROP_ENUMERATE),
	JS_PSG("domain", document_get_property_domain, JSPROP_ENUMERATE),
	JS_PSG("forms", document_get_property_forms, JSPROP_ENUMERATE),
	JS_PSG("head", document_get_property_head, JSPROP_ENUMERATE),
	JS_PSG("images", document_get_property_images, JSPROP_ENUMERATE),
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
static bool document_createElement(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_createTextNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_write(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_writeln(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_replace(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementById(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementsByClassName(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementsByName(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_getElementsByTagName(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec document_funcs[] = {
	{ "createComment",	document_createComment, 1 },
	{ "createElement",	document_createElement, 1 },
	{ "createTextNode",	document_createTextNode, 1 },
	{ "write",		document_write,		1 },
	{ "writeln",		document_writeln,	1 },
	{ "replace",		document_replace,	1 },
	{ "getElementById",	document_getElementById,	1 },
	{ "getElementsByClassName",	document_getElementsByClassName,	1 },
	{ "getElementsByName",	document_getElementsByName,	1 },
	{ "getElementsByTagName",	document_getElementsByTagName,	1 },
	{ NULL }
};

/* @document_class.getProperty */
static bool
document_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::RootedObject parent_win(ctx);	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form *form;
	char *string;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	JSClass* classPtr = JS_GetClass(hobj);

	if (classPtr != &document_class)
		return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	if (!JSID_IS_STRING(hid)) {
		return true;
	}
	string = jsid_to_string(ctx, hid);

	if (spidermonkey_check_if_function_name(document_funcs, string)) {
		return true;
	}

	foreach (form, document->forms) {
		if (!form->name || c_strcasecmp(string, form->name))
			continue;

		JSObject *obj = get_form_object(ctx, hobj, find_form_view(doc_view, form));

		if (obj) {
			hvp.setObject(*obj);
			return true;
		} else {
			hvp.setNull();
			return true;
		}
	}

	hvp.setUndefined();
	return false;
}

static bool
document_write_do(JSContext *ctx, unsigned int argc, JS::Value *rval, int newline)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	JS::Value val;
	struct string *ret = interpreter->ret;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

	struct string code;

	init_string(&code);

	if (argc >= 1)
	{
		for (int i=0;i<argc;++i) 
		{

			jshandle_value_to_char_string(&code,ctx,&args[i]);
		}
	
		if (newline) 
		{
			add_to_string(&code, "\n");
		}
	}
	//DBG("%s",code.source);

	/* XXX: I don't know about you, but I have *ENOUGH* of those 'Undefined
	 * function' errors, I want to see just the useful ones. So just
	 * lighting a led and going away, no muss, no fuss. --pasky */
	/* TODO: Perhaps we can introduce ecmascript.error_report_unsupported
	 * -> "Show information about the document using some valid,
	 *  nevertheless unsupported methods/properties." --pasky too */

	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document;
	document = doc_view->document;
	struct cache_entry *cached = doc_view->document->cached;
	cached = doc_view->document->cached;
	struct fragment *f = get_cache_fragment(cached);

	if (f && f->length)
	{
		if (false && document->ecmascript_counter==0)
		{
			add_fragment(cached,0,code.source,code.length);
		} else {
			add_fragment(cached,f->length,code.source,code.length);
		}
		document->ecmascript_counter++;
	}

#ifdef CONFIG_LEDS
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif

	done_string(&code);
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;
	struct terminal *term = ses->tab->term;
	struct string *ret = interpreter->ret;
	struct document *document;
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	document = doc_view->document;

	if (argc != 2) {
		args.rval().setBoolean(false);
		return(true);
	}

	struct string needle;
	struct string heystack;

	init_string(&needle);
	init_string(&heystack);

	jshandle_value_to_char_string(&needle, ctx, &args[0]);
	jshandle_value_to_char_string(&heystack, ctx, &args[1]);

	//DBG("doc replace %s %s\n", needle.source, heystack.source);

	int nu_len=0;
	int fd_len=0;
	unsigned char *nu;
	struct cache_entry *cached = doc_view->document->cached;
	struct fragment *f = get_cache_fragment(cached);

	if (f && f->length)
	{
		fd_len=f->length;

		struct string f_data;
		init_string(&f_data);
		add_to_string(&f_data,f->data);

		struct string nu_str;
		init_string(&nu_str);
		string_replace(&nu_str,&f_data,&needle,&heystack);
		nu_len=nu_str.length;
		delete_entry_content(cached);
		/* This is very ugly, indeed. And Yes fd_len isn't 
		 * logically correct. But using nu_len will cause
		 * the document to render improperly.
		 * TBD: somehow better rerender the document 
		 * now it's places on the session level in doc_loading_callback */
		int ret = add_fragment(cached,0,nu_str.source,fd_len);
		normalize_cache_entry(cached,nu_len);
		document->ecmascript_counter++;
		//DBG("doc replace %s %s\n", needle.source, heystack.source);
	}

	done_string(&needle);
	done_string(&heystack);

	args.rval().setBoolean(true);

	return(true);
}

void *
document_parse(struct document *document)
{
	struct cache_entry *cached = document->cached;
	struct fragment *f = get_cache_fragment(cached);

	if (!f || !f->length) {
		return NULL;
	}

	struct string str;
	init_string(&str);

	add_bytes_to_string(&str, f->data, f->length);

	// Parse HTML and create a DOM tree
	xmlDoc* doc = htmlReadDoc((xmlChar*)str.source, NULL, get_cp_mime_name(document->cp),
	HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	// Encapsulate raw libxml document in a libxml++ wrapper
	xmlpp::Document *docu = new xmlpp::Document(doc);
	done_string(&str);

	return (void *)docu;
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;

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
	init_string(&idstr);
	jshandle_value_to_char_string(&idstr, ctx, &args[0]);
	std::string text = idstr.source;
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;

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
	init_string(&idstr);
	jshandle_value_to_char_string(&idstr, ctx, &args[0]);
	std::string text = idstr.source;
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;

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
	init_string(&idstr);
	jshandle_value_to_char_string(&idstr, ctx, &args[0]);
	std::string text = idstr.source;
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	init_string(&idstr);
	jshandle_value_to_char_string(&idstr, ctx, &args[0]);
	std::string id = idstr.source;

	std::string xpath = "//*[@id=\"";
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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	init_string(&idstr);
	jshandle_value_to_char_string(&idstr, ctx, &args[0]);
	std::string id = idstr.source;

	std::string xpath = "//*[@class=\"";
	xpath += id;
	xpath += "\"]";

	done_string(&idstr);

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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	init_string(&idstr);
	jshandle_value_to_char_string(&idstr, ctx, &args[0]);
	std::string id = idstr.source;

	std::string xpath = "//*[@id=\"";
	xpath += id;
	xpath += "\"]|//*[@name=\"";
	xpath += id;
	xpath += "\"]";

	done_string(&idstr);

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

	JSCompartment *comp = js::GetContextCompartment(ctx);
	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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

	init_string(&idstr);
	jshandle_value_to_char_string(&idstr, ctx, &args[0]);
	std::string id = idstr.source;
	std::transform(id.begin(), id.end(), id.begin(), ::tolower);

	std::string xpath = "//";
	xpath += id;

	done_string(&idstr);

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

JSClassOps doctype_ops = {
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr
};


JSClass doctype_class = {
	"doctype",
	JSCLASS_HAS_PRIVATE,
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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &doctype_class, NULL))
		return false;

	xmlpp::Dtd *dtd = JS_GetPrivate(hobj);

	if (!dtd) {
		args.rval().setNull();
		return true;
	}

	std::string v = dtd->get_name();
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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &doctype_class, NULL))
		return false;

	xmlpp::Dtd *dtd = JS_GetPrivate(hobj);

	if (!dtd) {
		args.rval().setNull();
		return true;
	}

	std::string v = dtd->get_external_id();
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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &doctype_class, NULL))
		return false;

	xmlpp::Dtd *dtd = JS_GetPrivate(hobj);

	if (!dtd) {
		args.rval().setNull();
		return true;
	}

	std::string v = dtd->get_system_id();
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
	JS_SetPrivate(el, node);

	return el;
}
