/* The QuickJS document object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"

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
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/collection.h"
#include "ecmascript/quickjs/form.h"
#include "ecmascript/quickjs/forms.h"
#include "ecmascript/quickjs/implementation.h"
#include "ecmascript/quickjs/location.h"
#include "ecmascript/quickjs/document.h"
#include "ecmascript/quickjs/element.h"
#include "ecmascript/quickjs/nodelist.h"
#include "ecmascript/quickjs/window.h"
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

#include <algorithm>
#include <map>
#include <iostream>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static xmlpp::Document emptyDoc;
static JSValue getDoctype(JSContext *ctx, void *node);
static JSClassID js_doctype_class_id;
static JSClassID js_document_class_id;

static JSValue
js_document_get_property_anchors(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//a";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		return JS_NULL;
	}

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		return JS_NULL;
	}

	return getCollection(ctx, elements);
}

static JSValue
js_document_get_property_baseURI(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}

	char *str = get_uri_string(vs->uri, URI_BASE);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}

	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	RETURN_JS(ret);
}

static JSValue
js_document_get_property_body(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//body";
	xmlpp::Node::NodeSet elements = root->find(xpath);

	if (elements.size() == 0) {
		return JS_NULL;
	}

	auto element = elements[0];

	return getElement(ctx, element);
}

static JSValue
js_document_set_property_body(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);
	// TODO
	return JS_UNDEFINED;
}

#ifdef CONFIG_COOKIES
static JSValue
js_document_get_property_cookie(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs;
	struct string *cookies;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	cookies = send_cookies_js(vs->uri);

	if (cookies) {
		static char cookiestr[1024];

		strncpy(cookiestr, cookies->source, 1023);
		done_string(cookies);

		JSValue r = JS_NewString(ctx, cookiestr);

		RETURN_JS(r);
	} else {
		JSValue rr = JS_NewStringLen(ctx, "", 0);

		RETURN_JS(rr);
	}
}

static JSValue
js_document_set_property_cookie(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	const char *text;
	char *str;
	size_t len;
	text = JS_ToCStringLen(ctx, &len, val);

	if (!text) {
		return JS_EXCEPTION;
	}
	str = stracpy(text);
	if (str) {
		set_cookie(vs->uri, str);
		mem_free(str);
	}
	JS_FreeCString(ctx, text);

	return JS_UNDEFINED;
}

#endif

static JSValue
js_document_get_property_charset(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document* docu = (xmlpp::Document *)document->dom;
	xmlpp::ustring encoding = docu->get_encoding();

	if (encoding == "") {
		encoding = "utf-8";
	}

	JSValue r = JS_NewStringLen(ctx, encoding.c_str(), encoding.length());

	RETURN_JS(r);
}

static JSValue
js_document_get_property_childNodes(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	struct document *document = vs->doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	if (!root) {
		return JS_NULL;
	}

	xmlpp::Node::NodeList *nodes = new(std::nothrow) xmlpp::Node::NodeList;

	if (!nodes) {
		return JS_NULL;
	}

	*nodes = root->get_children();
	if (nodes->empty()) {
		delete nodes;
		return JS_NULL;
	}

	return getNodeList(ctx, nodes);
}

static JSValue
js_document_get_property_doctype(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document* docu = (xmlpp::Document *)document->dom;
	xmlpp::Dtd *dtd = docu->get_internal_subset();

	if (!dtd) {
		return JS_NULL;
	}

	return getDoctype(ctx, dtd);
}

static JSValue
js_document_get_property_documentElement(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//html";
	xmlpp::Node::NodeSet elements = root->find(xpath);

	if (elements.size() == 0) {
		return JS_NULL;
	}

	auto element = elements[0];

	return getElement(ctx, element);
}

static JSValue
js_document_get_property_documentURI(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
		return JS_NULL;
	}

	char *str = get_uri_string(vs->uri, URI_BASE);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}

	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	RETURN_JS(ret);
}

static JSValue
js_document_get_property_domain(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}

	char *str = get_uri_string(vs->uri, URI_HOST);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}

	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	RETURN_JS(ret);
}

static JSValue
js_document_get_property_forms(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	if (!document->forms_nodeset) {
		document->forms_nodeset = new(std::nothrow) xmlpp::Node::NodeSet;
	}

	if (!document->forms_nodeset) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	xmlpp::ustring xpath = "//form";
	xmlpp::Node::NodeSet *elements = static_cast<xmlpp::Node::NodeSet *>(document->forms_nodeset);
	*elements = root->find(xpath);

	if (elements->size() == 0) {
		return JS_NULL;
	}

	return getForms(ctx, elements);
}

static JSValue
js_document_get_property_head(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//head";
	xmlpp::Node::NodeSet elements = root->find(xpath);

	if (elements.size() == 0) {
		return JS_NULL;
	}

	auto element = elements[0];

	return getElement(ctx, element);
}

static JSValue
js_document_get_property_images(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//img";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		return JS_NULL;
	}

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		return JS_NULL;
	}

	return getCollection(ctx, elements);
}

static JSValue
js_document_get_property_implementation(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	return getImplementation(ctx);
}

static JSValue
js_document_get_property_links(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//a[@href]|//area[@href]";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		return JS_NULL;
	}

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		return JS_NULL;
	}

	return getCollection(ctx, elements);
}

static JSValue
js_document_get_property_location(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

	RETURN_JS(interpreter->location_obj);
}

static JSValue
js_document_get_property_nodeType(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewInt32(ctx, 9);
}

static JSValue
js_document_set_property_location(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs;
	struct document_view *doc_view;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	const char *url;
	size_t len;

	url = JS_ToCStringLen(ctx, &len, val);

	if (!url) {
		return JS_EXCEPTION;
	}

	location_goto_const(doc_view, url);
	JS_FreeCString(ctx, url);

	return JS_UNDEFINED;
}

static JSValue
js_document_get_property_referrer(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;

	switch (get_opt_int("protocol.http.referer.policy", NULL)) {
	case REFERER_NONE:
		/* oh well */
		return JS_UNDEFINED;

	case REFERER_FAKE:
		return JS_NewString(ctx, get_opt_str("protocol.http.referer.fake", NULL));

	case REFERER_TRUE:
		/* XXX: Encode as in add_url_to_httset_prop_string(&prop, ) ? --pasky */
		if (ses->referrer) {
			char *str = get_uri_string(ses->referrer, URI_HTTP_REFERRER);

			if (str) {
				JSValue ret = JS_NewString(ctx, str);
				mem_free(str);

				RETURN_JS(ret);
			} else {
				return JS_UNDEFINED;
			}
		}
		break;

	case REFERER_SAME_URL:
		char *str = get_uri_string(document->uri, URI_HTTP_REFERRER);

		if (str) {
			JSValue ret = JS_NewString(ctx, str);
			mem_free(str);

			RETURN_JS(ret);
		} else {
			return JS_UNDEFINED;
		}
		break;
	}

	return JS_UNDEFINED;
}

static JSValue
js_document_get_property_scripts(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//script";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		return JS_NULL;
	}

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		return JS_NULL;
	}

	return getCollection(ctx, elements);
}

static JSValue
js_document_get_property_title(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	JSValue r = JS_NewString(ctx, document->title);
	RETURN_JS(r);
}

static JSValue
js_document_set_property_title(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	vs = interpreter->vs;

	if (!vs || !vs->doc_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;

	const char *str;
	char *string;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}

	string = stracpy(str);
	JS_FreeCString(ctx, str);

	mem_free_set(&document->title, string);
	print_screen_status(doc_view->session);

	return JS_UNDEFINED;
}

static JSValue
js_document_get_property_url(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	char *str = get_uri_string(document->uri, URI_ORIGINAL);

	if (str) {
		JSValue ret = JS_NewString(ctx, str);
		mem_free(str);

		RETURN_JS(ret);
	} else {
		return JS_UNDEFINED;
	}
}

static JSValue
js_document_set_property_url(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs;
	struct document_view *doc_view;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	const char *url;
	size_t len;

	url = JS_ToCStringLen(ctx, &len, val);

	if (!url) {
		return JS_EXCEPTION;
	}
	location_goto_const(doc_view, url);
	JS_FreeCString(ctx, url);

	return JS_UNDEFINED;
}


static JSValue
js_document_write_do(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int newline)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

	if (argc >= 1) {
		int element_offset = interpreter->element_offset;
		struct string string;

		if (init_string(&string)) {
			for (int i = 0; i < argc; ++i) {
				const char *str;
				size_t len;

				str = JS_ToCStringLen(ctx, &len, argv[i]);

				if (!str) {
					done_string(&string);
					return JS_EXCEPTION;
				}
				add_bytes_to_string(&string, str, len);
				JS_FreeCString(ctx, str);
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
			interpreter->changed = true;
		}
	}

#ifdef CONFIG_LEDS
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif
	return JS_FALSE;
}

/* @document_funcs{"write"} */
static JSValue
js_document_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return js_document_write_do(ctx, this_val, argc, argv, 0);
}

/* @document_funcs{"writeln"} */
static JSValue
js_document_writeln(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return js_document_write_do(ctx, this_val, argc, argv, 1);
}

/* @document_funcs{"replace"} */
static JSValue
js_document_replace(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document;
	document = doc_view->document;

	if (argc != 2) {
		return JS_FALSE;
	}

	struct string needle;
	struct string heystack;

	if (!init_string(&needle)) {
		return JS_EXCEPTION;
	}
	if (!init_string(&heystack)) {
		done_string(&needle);
		return JS_EXCEPTION;
	}

	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (str) {
		add_bytes_to_string(&needle, str, len);
		JS_FreeCString(ctx, str);
	}

	str = JS_ToCStringLen(ctx, &len, argv[1]);

	if (str) {
		add_bytes_to_string(&heystack, str, len);
		JS_FreeCString(ctx, str);
	}
	//DBG("doc replace %s %s\n", needle.source, heystack.source);

	struct cache_entry *cached = doc_view->document->cached;
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

	return JS_TRUE;
}

static JSValue
js_document_createComment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	xmlpp::Element* emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		emptyDoc.create_root_node("root");
	}

	emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		return JS_NULL;
	}
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring text = str;
	JS_FreeCString(ctx, str);
	xmlpp::CommentNode *comment = emptyRoot->add_child_comment(text);

	if (!comment) {
		return JS_NULL;
	}

	return getElement(ctx, comment);
}

static JSValue
js_document_createDocumentFragment(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 0) {
		return JS_FALSE;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *doc2 = static_cast<xmlpp::Document *>(document->dom);
	xmlDoc *docu = doc2->cobj();
	xmlNode *xmlnode = xmlNewDocFragment(docu);

	if (!xmlnode) {
		return JS_NULL;
	}

	xmlpp::Node *node = new(std::nothrow) xmlpp::Node(xmlnode);

	if (!node) {
		return JS_NULL;
	}

	return getElement(ctx, node);
}

static JSValue
js_document_createElement(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	xmlpp::Element* emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		emptyDoc.create_root_node("root");
	}

	emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		return JS_NULL;
	}
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring text = str;
	JS_FreeCString(ctx, str);
	xmlpp::Element *elem = emptyRoot->add_child_element(text);

	if (!elem) {
		return JS_NULL;
	}

	return getElement(ctx, elem);
}

static JSValue
js_document_createTextNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	xmlpp::Element* emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		emptyDoc.create_root_node("root");
	}

	emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		return JS_NULL;
	}
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring text = str;
	JS_FreeCString(ctx, str);
	xmlpp::TextNode *textNode = emptyRoot->add_child_text(text);

	if (!textNode) {
		return JS_NULL;
	}

	return getElement(ctx, textNode);
}

static JSValue
js_document_getElementById(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring id = str;
	JS_FreeCString(ctx, str);
	xmlpp::ustring xpath = "//*[@id=\"";
	xpath += id;
	xpath += "\"]";

	auto elements = root->find(xpath);

	if (elements.size() == 0) {
		return JS_NULL;
	}

	auto node = elements[0];

	return getElement(ctx, node);
}

static JSValue
js_document_getElementsByClassName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring id = str;
	JS_FreeCString(ctx, str);

	xmlpp::ustring xpath = "//*[@class=\"";
	xpath += id;
	xpath += "\"]";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		return JS_NULL;
	}

	*elements = root->find(xpath);

	return getCollection(ctx, elements);
}

static JSValue
js_document_getElementsByName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring id = str;
	JS_FreeCString(ctx, str);
	xmlpp::ustring xpath = "//*[@id=\"";
	xpath += id;
	xpath += "\"]|//*[@name=\"";
	xpath += id;
	xpath += "\"]";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		return JS_NULL;
	}

	*elements = root->find(xpath);

	return getCollection(ctx, elements);
}

static JSValue
js_document_getElementsByTagName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring id = str;
	JS_FreeCString(ctx, str);
	std::transform(id.begin(), id.end(), id.begin(), ::tolower);

	xmlpp::ustring xpath = "//";
	xpath += id;
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		return JS_NULL;
	}

	*elements = root->find(xpath);

	return getCollection(ctx, elements);
}

static JSValue
js_document_querySelector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring css = str;
	JS_FreeCString(ctx, str);
	xmlpp::ustring xpath = css2xpath(css);

	xmlpp::Node::NodeSet elements;

	try {
		elements = root->find(xpath);
	} catch (xmlpp::exception &e) {
		return JS_NULL;
	}

	if (elements.size() == 0) {
		return JS_NULL;
	}

	auto node = elements[0];

	return getElement(ctx, node);
}

static JSValue
js_document_querySelectorAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return JS_NULL;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring css = str;
	JS_FreeCString(ctx, str);
	xmlpp::ustring xpath = css2xpath(css);
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		return JS_NULL;
	}

	try {
		*elements = root->find(xpath);
	} catch (xmlpp::exception &e) {
	}

	return getCollection(ctx, elements);
}

#if 0
JSClass doctype_class = {
	"doctype",
	JSCLASS_HAS_PRIVATE,
	&doctype_ops
};
#endif

static JSValue
js_doctype_get_property_name(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	xmlpp::Dtd *dtd = static_cast<xmlpp::Dtd *>(JS_GetOpaque(this_val, js_doctype_class_id));

	if (!dtd) {
		return JS_NULL;
	}
	xmlpp::ustring v = dtd->get_name();

	return JS_NewStringLen(ctx, v.c_str(), v.length());
}

static JSValue
js_doctype_get_property_publicId(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	xmlpp::Dtd *dtd = static_cast<xmlpp::Dtd *>(JS_GetOpaque(this_val, js_doctype_class_id));

	if (!dtd) {
		return JS_NULL;
	}
	xmlpp::ustring v = dtd->get_external_id();

	JSValue r = JS_NewStringLen(ctx, v.c_str(), v.length());
	RETURN_JS(r);
}

static JSValue
js_doctype_get_property_systemId(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	xmlpp::Dtd *dtd = static_cast<xmlpp::Dtd *>(JS_GetOpaque(this_val, js_doctype_class_id));

	if (!dtd) {
		return JS_NULL;
	}
	xmlpp::ustring v = dtd->get_system_id();

	JSValue r = JS_NewStringLen(ctx, v.c_str(), v.length());
	RETURN_JS(r);
}

static JSValue
js_document_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[document object]");
}

static const JSCFunctionListEntry js_document_proto_funcs[] = {
	JS_CGETSET_DEF("anchors", js_document_get_property_anchors, nullptr),
	JS_CGETSET_DEF("baseURI", js_document_get_property_baseURI, nullptr),
	JS_CGETSET_DEF("body", js_document_get_property_body, js_document_set_property_body),
#ifdef CONFIG_COOKIES
	JS_CGETSET_DEF("cookie", js_document_get_property_cookie, js_document_set_property_cookie),
#endif
	JS_CGETSET_DEF("charset", js_document_get_property_charset, nullptr),
	JS_CGETSET_DEF("characterSet", js_document_get_property_charset, nullptr),
	JS_CGETSET_DEF("childNodes", js_document_get_property_childNodes, nullptr),
	JS_CGETSET_DEF("doctype", js_document_get_property_doctype, nullptr),
	JS_CGETSET_DEF("documentElement", js_document_get_property_documentElement, nullptr),
	JS_CGETSET_DEF("documentURI", js_document_get_property_documentURI, nullptr),
	JS_CGETSET_DEF("domain", js_document_get_property_domain, nullptr),
	JS_CGETSET_DEF("forms", js_document_get_property_forms, nullptr),
	JS_CGETSET_DEF("head", js_document_get_property_head, nullptr),
	JS_CGETSET_DEF("images", js_document_get_property_images, nullptr),
	JS_CGETSET_DEF("implementation", js_document_get_property_implementation, nullptr),
	JS_CGETSET_DEF("inputEncoding", js_document_get_property_charset, nullptr),
	JS_CGETSET_DEF("links", js_document_get_property_links, nullptr),
	JS_CGETSET_DEF("location",	js_document_get_property_location, js_document_set_property_location),
	JS_CGETSET_DEF("nodeType", js_document_get_property_nodeType, nullptr),
	JS_CGETSET_DEF("referrer", js_document_get_property_referrer, nullptr),
	JS_CGETSET_DEF("scripts", js_document_get_property_scripts, nullptr),
	JS_CGETSET_DEF("title",	js_document_get_property_title, js_document_set_property_title), /* TODO: Charset? */
	JS_CGETSET_DEF("URL", js_document_get_property_url, js_document_set_property_url),

	JS_CFUNC_DEF("createComment",	1, js_document_createComment),
	JS_CFUNC_DEF("createDocumentFragment",	0, js_document_createDocumentFragment),
	JS_CFUNC_DEF("createElement",	1, js_document_createElement),
	JS_CFUNC_DEF("createTextNode",	1, js_document_createTextNode),
	JS_CFUNC_DEF("write",		1, js_document_write),
	JS_CFUNC_DEF("writeln",		1, js_document_writeln),
	JS_CFUNC_DEF("replace",		2, js_document_replace),
	JS_CFUNC_DEF("getElementById",	1, js_document_getElementById),
	JS_CFUNC_DEF("getElementsByClassName",	1, js_document_getElementsByClassName),
	JS_CFUNC_DEF("getElementsByName",	1, js_document_getElementsByName),
	JS_CFUNC_DEF("getElementsByTagName",	1, js_document_getElementsByTagName),
	JS_CFUNC_DEF("querySelector",	1, js_document_querySelector),
	JS_CFUNC_DEF("querySelectorAll",	1, js_document_querySelectorAll),

	JS_CFUNC_DEF("toString", 0, js_document_toString)
};

static JSClassDef js_document_class = {
	"document",
};

static JSValue
js_document_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	REF_JS(new_target);

	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	REF_JS(proto);

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_document_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	RETURN_JS(obj);

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

JSValue
js_document_init(JSContext *ctx)
{
	JSValue document_proto, document_class;

	/* create the document class */
	JS_NewClassID(&js_document_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_document_class_id, &js_document_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	document_proto = JS_NewObject(ctx);
	REF_JS(document_proto);

	JS_SetPropertyFunctionList(ctx, document_proto, js_document_proto_funcs, countof(js_document_proto_funcs));

	document_class = JS_NewCFunction2(ctx, js_document_ctor, "document", 0, JS_CFUNC_constructor, 0);
	REF_JS(document_class);

	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, document_class, document_proto);
	JS_SetClassProto(ctx, js_document_class_id, document_proto);

	JS_SetPropertyStr(ctx, global_obj, "document", document_proto);

	JS_FreeValue(ctx, global_obj);

	RETURN_JS(document_proto);
}

static JSValue
js_doctype_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[doctype object]");
}

static const JSCFunctionListEntry js_doctype_proto_funcs[] = {
	JS_CGETSET_DEF("name", js_doctype_get_property_name, nullptr),
	JS_CGETSET_DEF("publicId", js_doctype_get_property_publicId, nullptr),
	JS_CGETSET_DEF("systemId", js_doctype_get_property_systemId, nullptr),
	JS_CFUNC_DEF("toString", 0, js_doctype_toString)
};

static std::map<void *, JSValueConst> map_doctypes;

static void
js_doctype_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	void *node = JS_GetOpaque(val, js_doctype_class_id);
	map_doctypes.erase(node);
}

static JSClassDef js_doctype_class = {
	"doctype",
	js_doctype_finalizer
};

static JSValue
js_doctype_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	REF_JS(new_target);
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	REF_JS(proto);

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_doctype_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	RETURN_JS(obj);

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_doctype_init(JSContext *ctx)
{
	JSValue doctype_proto, doctype_class;

	/* create the doctype class */
	JS_NewClassID(&js_doctype_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_doctype_class_id, &js_doctype_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	doctype_proto = JS_NewObject(ctx);
	REF_JS(doctype_proto);

	JS_SetPropertyFunctionList(ctx, doctype_proto, js_doctype_proto_funcs, countof(js_doctype_proto_funcs));

	doctype_class = JS_NewCFunction2(ctx, js_doctype_ctor, "doctype", 0, JS_CFUNC_constructor, 0);
	REF_JS(doctype_class);

	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, doctype_class, doctype_proto);
	JS_SetClassProto(ctx, js_doctype_class_id, doctype_proto);

	JS_SetPropertyStr(ctx, global_obj, "doctype", doctype_proto);

	JS_FreeValue(ctx, global_obj);

	return 0;
}

static JSValue
getDoctype(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	static int initialized;
	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_doctype_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_doctype_class_id, &js_doctype_class);
		initialized = 1;
		map_doctypes.clear();
	}
	auto node_find = map_doctypes.find(node);

	if (node_find != map_doctypes.end()) {
		JSValue r = JS_DupValue(ctx, node_find->second);
		RETURN_JS(r);
	}
	JSValue doctype_obj = JS_NewObjectClass(ctx, js_doctype_class_id);
	JS_SetPropertyFunctionList(ctx, doctype_obj, js_doctype_proto_funcs, countof(js_doctype_proto_funcs));
	JS_SetClassProto(ctx, js_doctype_class_id, doctype_obj);
	JS_SetOpaque(doctype_obj, node);

	map_doctypes[node] = doctype_obj;

	JSValue rr = JS_DupValue(ctx, doctype_obj);
	RETURN_JS(rr);
}

JSValue
getDocument(JSContext *ctx, void *doc)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue document_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, document_obj, js_document_proto_funcs, countof(js_document_proto_funcs));
//	document_class = JS_NewCFunction2(ctx, js_document_ctor, "document", 0, JS_CFUNC_constructor, 0);
//	JS_SetConstructor(ctx, document_class, document_obj);
	JS_SetClassProto(ctx, js_document_class_id, document_obj);
	JS_SetOpaque(document_obj, doc);

	RETURN_JS(document_obj);
}
