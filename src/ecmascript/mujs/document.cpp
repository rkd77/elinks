/* The MuJS document object implementation. */

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
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/collection.h"
#include "ecmascript/mujs/form.h"
#include "ecmascript/mujs/forms.h"
#include "ecmascript/mujs/implementation.h"
#include "ecmascript/mujs/location.h"
#include "ecmascript/mujs/document.h"
#include "ecmascript/mujs/element.h"
#include "ecmascript/mujs/nodelist.h"
#include "ecmascript/mujs/window.h"
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

static xmlpp::Document emptyDoc;
static void mjs_push_doctype(js_State *J, void *node);

static void
mjs_document_get_property_anchors(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//a";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		js_pushnull(J);
		return;
	}

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		js_pushnull(J);
		return;
	}
	mjs_push_collection(J, elements);
}

static void
mjs_document_get_property_baseURI(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}

	char *str = get_uri_string(vs->uri, URI_BASE);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_document_get_property_body(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//body";
	xmlpp::Node::NodeSet elements = root->find(xpath);

	if (elements.size() == 0) {
		js_pushnull(J);
		return;
	}
	auto element = elements[0];
	mjs_push_element(J, element);
}

static void
mjs_document_set_property_body(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	// TODO
	js_pushundefined(J);
}

#ifdef CONFIG_COOKIES
static void
mjs_document_get_property_cookie(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs;
	struct string *cookies;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	cookies = send_cookies_js(vs->uri);

	if (cookies) {
		static char cookiestr[1024];

		strncpy(cookiestr, cookies->source, 1023);
		done_string(cookies);
		js_pushstring(J, cookiestr);
		return;

	} else {
		js_pushstring(J, "");
		return;
	}
}

static void
mjs_document_set_property_cookie(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	const char *text = js_tostring(J, 1);

	if (!text) {
		js_pushnull(J);
		return;
	}
	char *str = stracpy(text);
	if (str) {
		set_cookie(vs->uri, str);
		mem_free(str);
	}
	js_pushundefined(J);
}

#endif

static void
mjs_document_get_property_charset(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document* docu = (xmlpp::Document *)document->dom;
	xmlpp::ustring encoding = docu->get_encoding();

	if (encoding == "") {
		encoding = "utf-8";
	}
	js_pushstring(J, encoding.c_str());
}

static void
mjs_document_get_property_childNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	struct document *document = vs->doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	if (!root) {
		js_pushnull(J);
		return;
	}

	xmlpp::Node::NodeList *nodes = new(std::nothrow) xmlpp::Node::NodeList;

	if (!nodes) {
		js_pushnull(J);
		return;
	}

	*nodes = root->get_children();
	if (nodes->empty()) {
		delete nodes;
		js_pushnull(J);
		return;
	}
	mjs_push_nodelist(J, nodes);
}

static void
mjs_document_get_property_doctype(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document* docu = (xmlpp::Document *)document->dom;
	xmlpp::Dtd *dtd = docu->get_internal_subset();

	if (!dtd) {
		js_pushnull(J);
		return;
	}
	mjs_push_doctype(J, dtd);
}

static void
mjs_document_get_property_documentElement(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//html";
	xmlpp::Node::NodeSet elements = root->find(xpath);

	if (elements.size() == 0) {
		js_pushnull(J);
		return;
	}
	auto element = elements[0];
	mjs_push_element(J, element);
}

static void
mjs_document_get_property_documentURI(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
		js_pushnull(J);
		return;
	}

	char *str = get_uri_string(vs->uri, URI_BASE);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_document_get_property_domain(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}

	char *str = get_uri_string(vs->uri, URI_HOST);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_document_get_property_forms(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	if (!document->forms_nodeset) {
		document->forms_nodeset = new(std::nothrow) xmlpp::Node::NodeSet;
	}

	if (!document->forms_nodeset) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	xmlpp::ustring xpath = "//form";
	xmlpp::Node::NodeSet *elements = static_cast<xmlpp::Node::NodeSet *>(document->forms_nodeset);
	*elements = root->find(xpath);

	if (elements->size() == 0) {
		js_pushnull(J);
		return;
	}
	mjs_push_forms(J, elements);
}

static void
mjs_document_get_property_head(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//head";
	xmlpp::Node::NodeSet elements = root->find(xpath);

	if (elements.size() == 0) {
		js_pushnull(J);
		return;
	}
	auto element = elements[0];
	mjs_push_element(J, element);
}

static void
mjs_document_get_property_images(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//img";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		js_pushnull(J);
		return;
	}

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		js_pushnull(J);
		return;
	}
	mjs_push_collection(J, elements);
}

static void
mjs_document_get_property_implementation(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}
	mjs_push_implementation(J);
}

static void
mjs_document_get_property_links(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//a[@href]|//area[@href]";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		js_pushnull(J);
		return;
	}

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		js_pushnull(J);
		return;
	}
	mjs_push_collection(J, elements);
}

static void
mjs_document_get_property_nodeType(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, 9);
}

static void
mjs_document_get_property_referrer(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;

	switch (get_opt_int("protocol.http.referer.policy", NULL)) {
	case REFERER_NONE:
		/* oh well */
		js_pushundefined(J);
		return;

	case REFERER_FAKE:
		js_pushstring(J, get_opt_str("protocol.http.referer.fake", NULL));
		return;

	case REFERER_TRUE:
		/* XXX: Encode as in add_url_to_httset_prop_string(&prop, ) ? --pasky */
		if (ses->referrer) {
			char *str = get_uri_string(ses->referrer, URI_HTTP_REFERRER);

			if (str) {
				js_pushstring(J, str);
				mem_free(str);
				return;
			} else {
				js_pushundefined(J);
				return;
			}
		}
		break;

	case REFERER_SAME_URL:
		char *str = get_uri_string(document->uri, URI_HTTP_REFERRER);

		if (str) {
			js_pushstring(J, str);
			mem_free(str);
			return;
		} else {
			js_pushundefined(J);
			return;
		}
		break;
	}
	js_pushundefined(J);
}

static void
mjs_document_get_property_scripts(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	xmlpp::ustring xpath = "//script";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		js_pushnull(J);
		return;
	}

	*elements = root->find(xpath);

	if (elements->size() == 0) {
		js_pushnull(J);
		return;
	}
	mjs_push_collection(J, elements);
}

static void
mjs_document_get_property_title(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	js_pushstring(J, document->title);
}

static void
mjs_document_set_property_title(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	vs = interpreter->vs;

	if (!vs || !vs->doc_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;

	const char *str = js_tostring(J, 1);
	char *string;

	if (!str) {
		js_pushnull(J);
		return;
	}
	string = stracpy(str);
	mem_free_set(&document->title, string);
	print_screen_status(doc_view->session);
	js_pushundefined(J);
}

static void
mjs_document_get_property_url(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	char *str = get_uri_string(document->uri, URI_ORIGINAL);

	if (str) {
		js_pushstring(J, str);
		mem_free(str);
		return;
	} else {
		js_pushundefined(J);
		return;
	}
}

static void
mjs_document_set_property_url(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs;
	struct document_view *doc_view;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	doc_view = vs->doc_view;
	const char *url = js_tostring(J, 1);

	if (!url) {
		js_pushnull(J);
		return;
	}
	location_goto_const(doc_view, url);
	js_pushundefined(J);
}

static void
mjs_document_write_do(js_State *J, int newline)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	int argc = 1;

	if (argc >= 1)
	{
		for (int i = 0; i < argc; ++i)
		{
			const char *str = js_tostring(J, i+1);

			if (str) {
				add_to_string(&interpreter->writecode, str);
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
	js_pushundefined(J);
}

/* @document_funcs{"write"} */
static void
mjs_document_write(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_document_write_do(J, 0);
}

/* @document_funcs{"writeln"} */
static void
mjs_document_writeln(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_document_write_do(J, 1);
}

/* @document_funcs{"replace"} */
static void
mjs_document_replace(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document;
	document = doc_view->document;

	struct string needle;
	struct string heystack;

	if (!init_string(&needle)) {
		js_pushnull(J);
		return;
	}
	if (!init_string(&heystack)) {
		done_string(&needle);
		js_pushnull(J);
		return;
	}

	const char *str = js_tostring(J, 1);

	if (str) {
		add_to_string(&needle, str);
	}

	str = js_tostring(J, 2);

	if (str) {
		add_to_string(&heystack, str);
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

	js_pushundefined(J);
}

static void
mjs_document_createComment(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element* emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		emptyDoc.create_root_node("root");
	}

	emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring text = str;
	xmlpp::CommentNode *comment = emptyRoot->add_child_comment(text);

	if (!comment) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, comment);
}

static void
mjs_document_createDocumentFragment(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *doc2 = static_cast<xmlpp::Document *>(document->dom);
	xmlDoc *docu = doc2->cobj();
	xmlNode *xmlnode = xmlNewDocFragment(docu);

	if (!xmlnode) {
		js_pushnull(J);
		return;
	}

	xmlpp::Node *node = new(std::nothrow) xmlpp::Node(xmlnode);

	if (!node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
}

static void
mjs_document_createElement(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element* emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		emptyDoc.create_root_node("root");
	}

	emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring text = str;
	xmlpp::Element *elem = emptyRoot->add_child_element(text);

	if (!elem) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, elem);
}

static void
mjs_document_createTextNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element* emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		emptyDoc.create_root_node("root");
	}

	emptyRoot = (xmlpp::Element *)emptyDoc.get_root_node();

	if (!emptyRoot) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring text = str;
	xmlpp::TextNode *textNode = emptyRoot->add_child_text(text);

	if (!textNode) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, textNode);
}

static void
mjs_document_getElementById(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring id = str;
	xmlpp::ustring xpath = "//*[@id=\"";
	xpath += id;
	xpath += "\"]";

	auto elements = root->find(xpath);

	if (elements.size() == 0) {
		js_pushnull(J);
		return;
	}
	auto node = elements[0];
	mjs_push_element(J, node);
}

static void
mjs_document_getElementsByClassName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring id = str;
	xmlpp::ustring xpath = "//*[@class=\"";
	xpath += id;
	xpath += "\"]";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		js_pushnull(J);
		return;
	}
	*elements = root->find(xpath);
	mjs_push_collection(J, elements);
}

static void
mjs_document_getElementsByName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring id = str;
	xmlpp::ustring xpath = "//*[@id=\"";
	xpath += id;
	xpath += "\"]|//*[@name=\"";
	xpath += id;
	xpath += "\"]";
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		js_pushnull(J);
		return;
	}
	*elements = root->find(xpath);
	mjs_push_collection(J, elements);
}

static void
mjs_document_getElementsByTagName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring id = str;
	std::transform(id.begin(), id.end(), id.begin(), ::tolower);
	xmlpp::ustring xpath = "//";
	xpath += id;
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		js_pushnull(J);
		return;
	}
	*elements = root->find(xpath);
	mjs_push_collection(J, elements);
}

static void
mjs_document_querySelector(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring css = str;
	xmlpp::ustring xpath = css2xpath(css);

	xmlpp::Node::NodeSet elements;

	try {
		elements = root->find(xpath);
	} catch (xmlpp::exception &e) {
		js_pushnull(J);
		return;
	}

	if (elements.size() == 0) {
		js_pushnull(J);
		return;
	}
	auto node = elements[0];
	mjs_push_element(J, node);
}

static void
mjs_document_querySelectorAll(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		document->dom = document_parse(document);
	}

	if (!document->dom) {
		js_pushnull(J);
		return;
	}

	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring css = str;
	xmlpp::ustring xpath = css2xpath(css);
	xmlpp::Node::NodeSet *elements = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!elements) {
		js_pushnull(J);
		return;
	}

	try {
		*elements = root->find(xpath);
	} catch (xmlpp::exception &e) {
	}
	mjs_push_collection(J, elements);
}


static void
mjs_doctype_get_property_name(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Dtd *dtd = static_cast<xmlpp::Dtd *>(js_touserdata(J, 0, "doctype"));

	if (!dtd) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = dtd->get_name();
	js_pushstring(J, v.c_str());
}

static void
mjs_doctype_get_property_publicId(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Dtd *dtd = static_cast<xmlpp::Dtd *>(js_touserdata(J, 0, "doctype"));

	if (!dtd) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = dtd->get_external_id();
	js_pushstring(J, v.c_str());
}

static void
mjs_doctype_get_property_systemId(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Dtd *dtd = static_cast<xmlpp::Dtd *>(js_touserdata(J, 0, "doctype"));

	if (!dtd) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = dtd->get_system_id();
	js_pushstring(J, v.c_str());
}

static void
mjs_document_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[document object]");
}

int
mjs_document_init(js_State *J)
{
	js_newobject(J);
	{
		addmethod(J, "createComment",	mjs_document_createComment, 1);
		addmethod(J, "createDocumentFragment",mjs_document_createDocumentFragment, 0);
		addmethod(J, "createElement",	mjs_document_createElement, 1);
		addmethod(J, "createTextNode",	mjs_document_createTextNode, 1);
		addmethod(J, "write",		mjs_document_write, 1);
		addmethod(J, "writeln",		mjs_document_writeln, 1);
		addmethod(J, "replace",		mjs_document_replace, 2);
		addmethod(J, "getElementById",	mjs_document_getElementById, 1);
		addmethod(J, "getElementsByClassName",	mjs_document_getElementsByClassName, 1);
		addmethod(J, "getElementsByName",	mjs_document_getElementsByName, 1);
		addmethod(J, "getElementsByTagName",	mjs_document_getElementsByTagName, 1);
		addmethod(J, "querySelector",	mjs_document_querySelector, 1);
		addmethod(J, "querySelectorAll",	mjs_document_querySelectorAll, 1);
		addmethod(J, "toString", mjs_document_toString, 0);

		addproperty(J, "anchors", mjs_document_get_property_anchors, NULL);
		addproperty(J, "baseURI", mjs_document_get_property_baseURI, NULL);
		addproperty(J, "body", mjs_document_get_property_body, mjs_document_set_property_body);
#ifdef CONFIG_COOKIES
		addproperty(J, "cookie", mjs_document_get_property_cookie, mjs_document_set_property_cookie);
#endif
		addproperty(J, "charset", mjs_document_get_property_charset, NULL);
		addproperty(J, "characterSet", mjs_document_get_property_charset, NULL);
		addproperty(J, "childNodes", mjs_document_get_property_childNodes, NULL);
		addproperty(J, "doctype", mjs_document_get_property_doctype, NULL);
		addproperty(J, "documentElement", mjs_document_get_property_documentElement, NULL);
		addproperty(J, "documentURI", mjs_document_get_property_documentURI, NULL);
		addproperty(J, "domain", mjs_document_get_property_domain, NULL);
		addproperty(J, "forms", mjs_document_get_property_forms, NULL);
		addproperty(J, "head", mjs_document_get_property_head, NULL);
		addproperty(J, "images", mjs_document_get_property_images, NULL);
		addproperty(J, "implementation", mjs_document_get_property_implementation, NULL);
		addproperty(J, "inputEncoding", mjs_document_get_property_charset, NULL);
		addproperty(J, "links", mjs_document_get_property_links, NULL);
		addproperty(J, "nodeType", mjs_document_get_property_nodeType, NULL);
		addproperty(J, "referrer", mjs_document_get_property_referrer, NULL);
		addproperty(J, "scripts", mjs_document_get_property_scripts, NULL);
		addproperty(J, "title",	mjs_document_get_property_title, mjs_document_set_property_title); /* TODO: Charset? */
		addproperty(J, "URL", mjs_document_get_property_url, mjs_document_set_property_url);

	}
	js_defglobal(J, "document", JS_DONTENUM);
	js_dostring(J, "document.location = location; window.document = document;");

	return 0;
}

static void
mjs_doctype_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[doctype object]");
}

static std::map<void *, void *> map_doctypes;

static void
mjs_doctype_finalizer(js_State *J, void *node)
{
	map_doctypes.erase(node);
}

static void
mjs_push_doctype(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newobject(J);
	{
		js_newuserdata(J, "doctype", node, mjs_doctype_finalizer);
		addmethod(J, "toString", mjs_doctype_toString, 0);
		addproperty(J, "name", mjs_doctype_get_property_name, NULL);
		addproperty(J, "publicId", mjs_doctype_get_property_publicId, NULL);
		addproperty(J, "systemId", mjs_doctype_get_property_systemId, NULL);
	}
	map_doctypes[node] = node;
}

void
mjs_push_document(js_State *J, void *doc)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_document_init(J);
	js_newuserdata(J, "document", doc, NULL);
}
