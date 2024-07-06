/* The MuJS document object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

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
#include "document/libdom/doc.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/mujs/mapa.h"
#include "ecmascript/libdom/parse.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/collection.h"
#include "ecmascript/mujs/event.h"
#include "ecmascript/mujs/form.h"
#include "ecmascript/mujs/forms.h"
#include "ecmascript/mujs/implementation.h"
#include "ecmascript/mujs/location.h"
#include "ecmascript/mujs/document.h"
#include "ecmascript/mujs/element.h"
#include "ecmascript/mujs/nodelist.h"
#include "ecmascript/mujs/nodelist2.h"
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

//static xmlpp::Document emptyDoc;

struct el_listener {
	LIST_HEAD_EL(struct el_listener);
	char *typ;
	const char *fun;
};

enum readyState {
	LOADING = 0,
	INTERACTIVE,
	COMPLETE
};

struct mjs_document_private {
	struct ecmascript_interpreter *interpreter;
	const char *thisval;
	LIST_OF(struct el_listener) listeners;
	dom_event_listener *listener;
	void *node;
	int ref_count;
	enum readyState state;
};

static void document_event_handler(dom_event *event, void *pw);

static void *
mjs_doc_getprivate(js_State *J, int idx)
{
	struct mjs_document_private *priv = (struct mjs_document_private *)js_touserdata(J, idx, "document");

	if (!priv) {
		return NULL;
	}

	return priv->node;
}

static void mjs_push_doctype(js_State *J, void *node);

static void mjs_document_dispatchEvent(js_State *J);

static void
mjs_document_get_property_anchors(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_html_document *doc = (dom_html_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_html_collection *anchors = NULL;
	dom_exception exc = dom_html_document_get_anchors(doc, &anchors);

	if (exc != DOM_NO_ERR || !anchors) {
		js_pushnull(J);
		return;
	}
	mjs_push_collection(J, anchors);
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
	dom_html_document *doc = (dom_html_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_html_element *body = NULL;
	dom_exception exc = dom_html_document_get_body(doc, &body);

	if (exc != DOM_NO_ERR || !body) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, body);
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
		js_error(J, "!vs");
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
		js_error(J, "!vs");
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
// TODO
	js_pushstring(J, "utf-8");
}

static void
mjs_document_get_property_childNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_html_document *doc = (dom_html_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_element *root = NULL;
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR || !root) {
		js_pushnull(J);
		return;
	}
	dom_nodelist *nodes = NULL;
	exc = dom_node_get_child_nodes(root, &nodes);
	dom_node_unref(root);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	mjs_push_nodelist(J, nodes);
}

static void
mjs_document_get_property_defaultView(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushglobal(J);
}

static void
mjs_document_get_property_doctype(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_html_document *doc = (dom_html_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushundefined(J);
		return;
	}
	dom_document_type *dtd;
	dom_document_get_doctype(doc, &dtd);
	mjs_push_doctype(J, dtd);
}

static void
mjs_document_get_property_documentElement(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_html_document *doc = (dom_html_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_html_element *root = NULL;
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR || !root) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, root);
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
		js_error(J, "!str");
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
		js_error(J, "!vs");
		return;
	}

	char *str = get_uri_string(vs->uri, URI_HOST);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!str");
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
	dom_html_document *doc = (dom_html_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_html_collection *forms = NULL;
	dom_exception exc = dom_html_document_get_forms(doc, &forms);

	if (exc != DOM_NO_ERR || !forms) {
		js_pushnull(J);
		return;
	}
	mjs_push_forms(J, forms);
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
		js_pushnull(J);
		return;
	}
// TODO
#if 0
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
#endif
	js_pushnull(J);
}

static void
mjs_document_get_property_images(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_html_document *doc = (dom_html_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_html_collection *images = NULL;
	dom_exception exc = dom_html_document_get_images(doc, &images);

	if (exc != DOM_NO_ERR || !images) {
		js_pushnull(J);
		return;
	}
	mjs_push_collection(J, images);
}

static void
mjs_document_get_property_implementation(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_push_implementation(J);
}

static void
mjs_document_get_property_links(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_html_document *doc = (dom_html_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_html_collection *links = NULL;
	dom_exception exc = dom_html_document_get_links(doc, &links);

	if (exc != DOM_NO_ERR || !links) {
		js_pushnull(J);
		return;
	}
	mjs_push_collection(J, links);
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
mjs_document_get_property_readyState(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_document_private *doc_private = (struct mjs_document_private *)js_touserdata(J, 0, "document");

	if (!doc_private) {
		js_pushnull(J);
		return;
	}
	switch (doc_private->state) {
	case LOADING:
		js_pushstring(J, "loading");
		break;
	case INTERACTIVE:
		js_pushstring(J, "interactive");
		break;
	case COMPLETE:
		js_pushstring(J, "complete");
		break;
	}
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
	char *str;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
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
			str = get_uri_string(ses->referrer, URI_HTTP_REFERRER);

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
		str = get_uri_string(document->uri, URI_HTTP_REFERRER);

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
		js_pushnull(J);
		return;
	}
// TODO
#if 0
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
#endif
	js_pushnull(J);
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
		js_error(J, "!vs");
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
		js_error(J, "!vs || !doc_view");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;

	const char *str = js_tostring(J, 1);
	char *string;

	if (!str) {
		js_error(J, "!str");
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
		js_error(J, "!vs");
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
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	const char *url = js_tostring(J, 1);

	if (!url) {
		js_error(J, "!url");
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

	if (argc >= 1) {
		int element_offset = interpreter->element_offset;
		struct string string;

		if (init_string(&string)) {
			for (int i = 0; i < argc; ++i) {
				const char *str = js_tostring(J, i+1);

				if (str) {
					add_to_string(&string, str);
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
		}
	}

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
		js_error(J, "out of memory");
		return;
	}
	if (!init_string(&heystack)) {
		done_string(&needle);
		js_error(J, "out of memory");
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

	js_pushundefined(J);
}

static void
mjs_document_addEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_document_private *doc_private = (struct mjs_document_private *)js_touserdata(J, 0, "document");

	if (!doc_private) {
		js_pushnull(J);
		return;
	}
	dom_document *doc = (dom_document *)doc_private->node;

	if (!doc) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_error(J, "out of memory");
		return;
	}
	js_copy(J, 2);
	const char *fun = js_ref(J);
	struct el_listener *l;

	foreach(l, doc_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}

		if (l->fun == fun) {
			mem_free(method);
			js_pushundefined(J);
			return;
		}
	}
	struct el_listener *n = (struct el_listener *)mem_calloc(1, sizeof(*n));

	if (!n) {
		js_pushundefined(J);
		return;
	}
	n->fun = fun;
	n->typ = method;
	add_to_list_end(doc_private->listeners, n);
	dom_exception exc;

	if (doc_private->listener) {
		dom_event_listener_ref(doc_private->listener);
	} else {
		exc = dom_event_listener_create(document_event_handler, doc_private, &doc_private->listener);

		if (exc != DOM_NO_ERR || !doc_private->listener) {
			js_pushundefined(J);
			return;
		}
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

	js_pushundefined(J);
}

static void
mjs_document_removeEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_document_private *doc_private = (struct mjs_document_private *)js_touserdata(J, 0, "document");

	if (!doc_private) {
		js_pushnull(J);
		return;
	}
	dom_document *doc = (dom_document *)doc_private->node;

	if (!doc) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_error(J, "out of memory");
		return;
	}
	js_copy(J, 2);
	const char *fun = js_ref(J);
	struct el_listener *l;

	foreach(l, doc_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (l->fun == fun) {
			dom_string *typ = NULL;
			dom_exception exc = dom_string_create(method, strlen(method), &typ);

			if (exc != DOM_NO_ERR || !typ) {
				continue;
			}
			//dom_event_target_remove_event_listener(doc, typ, doc_private->listener, false);
			dom_string_unref(typ);

			js_unref(J, l->fun);
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			mem_free(l);
			mem_free(method);

			js_pushundefined(J);
			return;
		}
	}
	mem_free(method);
	js_pushundefined(J);
}

static void
mjs_document_createComment(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_string *data = NULL;
	dom_exception exc;
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &data);

	if (exc != DOM_NO_ERR || !data) {
		js_pushnull(J);
		return;
	}
	dom_comment *comment = NULL;
	exc = dom_document_create_comment(doc, data, &comment);
	dom_string_unref(data);

	if (exc != DOM_NO_ERR || !comment) {
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
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_document_fragment *fragment = NULL;
	dom_exception exc = dom_document_create_document_fragment(doc, &fragment);

	if (exc != DOM_NO_ERR || !fragment) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, fragment);
}

static void
mjs_document_createElement(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);
	dom_string *tag_name = NULL;
	dom_exception exc;
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		js_pushnull(J);
		return;
	}
	dom_element *element = NULL;
	exc = dom_document_create_element(doc, tag_name, &element);
	dom_string_unref(tag_name);

	if (exc != DOM_NO_ERR || !element) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, element);
}

static void
mjs_document_createTextNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);
	dom_string *data = NULL;
	dom_exception exc;
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &data);

	if (exc != DOM_NO_ERR || !data) {
		js_pushnull(J);
		return;
	}
	dom_text *text_node = NULL;
	exc = dom_document_create_text_node(doc, data, &text_node);
	dom_string_unref(data);

	if (exc != DOM_NO_ERR || !text_node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, text_node);
}

static void
mjs_document_getElementById(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_string *id = NULL;
	dom_exception exc;
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &id);

	if (exc != DOM_NO_ERR || !id) {
		js_pushnull(J);
		return;
	}
	dom_element *element = NULL;
	exc = dom_document_get_element_by_id(doc, id, &element);
	dom_string_unref(id);

	if (exc != DOM_NO_ERR || !element) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, element);
}

static void
mjs_document_getElementsByClassName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	const char *classes = js_tostring(J, 1);
	dom_html_collection *col = get_elements_by_class_name(doc, (dom_node *)doc, classes);

	if (!col) {
		js_pushnull(J);
		return;
	}
	mjs_push_collection(J, col);
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
		js_pushnull(J);
		return;
	}
// TODO
#if 0
	xmlpp::Document *docu = (xmlpp::Document *)document->dom;
	xmlpp::Element* root = (xmlpp::Element *)docu->get_root_node();

	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
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
#endif
	js_pushnull(J);
}

static void
mjs_document_getElementsByTagName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_string *tagname = NULL;
	dom_exception exc;
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &tagname);

	if (exc != DOM_NO_ERR || !tagname) {
		js_pushnull(J);
		return;
	}
	dom_nodelist *nodes = NULL;
	exc = dom_document_get_elements_by_tag_name(doc, tagname, &nodes);
	dom_string_unref(tagname);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	mjs_push_nodelist(J, nodes);
}

static void
mjs_document_querySelector(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_node *root = NULL; /* root element of document */
	/* Get root element */
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	const char *selector = js_tostring(J, 1);

	if (!selector) {
		dom_node_unref(root);
		js_pushnull(J);
		return;
	}
	void *ret = walk_tree_query(root, selector, 0);
	dom_node_unref(root);

	if (!ret) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, ret);
}

static void
mjs_document_querySelectorAll(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushnull(J);
		return;
	}
	dom_node *doc_root = NULL; /* root element of document */
	/* Get root element */
	dom_exception exc = dom_document_get_document_element(doc, &doc_root);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	const char *selector = js_tostring(J, 1);

	if (!selector) {
		dom_node_unref(doc_root);
		js_pushnull(J);
		return;
	}

	dom_string *tag_name = NULL;
	exc = dom_string_create((const uint8_t *)"B", 1, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		dom_node_unref(doc_root);
		js_pushnull(J);
		return;
	}
	dom_element *element = NULL;
	exc = dom_document_create_element(doc, tag_name, &element);
	dom_string_unref(tag_name);

	if (exc != DOM_NO_ERR || !element) {
		dom_node_unref(doc_root);
		js_pushnull(J);
		return;
	}
	LIST_OF(struct selector_node) *result_list = (LIST_OF(struct selector_node) *)mem_calloc(1, sizeof(*result_list));

	if (!result_list) {
		dom_node_unref(doc_root);
		js_pushnull(J);
		return;
	}
	init_list(*result_list);
	walk_tree_query_append((dom_node *)element, doc_root, selector, 0, result_list);
	dom_node_unref(doc_root);
	mjs_push_nodelist2(J, result_list);
}

static void
mjs_doctype_get_property_name(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_document_type *dtd = (dom_document_type *)(js_touserdata(J, 0, "doctype"));

	if (!dtd) {
		js_pushnull(J);
		return;
	}
	dom_string *name = NULL;
	dom_exception exc = dom_document_type_get_name(dtd, &name);

	if (exc != DOM_NO_ERR || !name) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(name));
	dom_string_unref(name);
}

static void
mjs_doctype_get_property_publicId(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_document_type *dtd = (dom_document_type *)(js_touserdata(J, 0, "doctype"));

	if (!dtd) {
		js_pushnull(J);
		return;
	}
	dom_string *public_id = NULL;
	dom_exception exc = dom_document_type_get_public_id(dtd, &public_id);

	if (exc != DOM_NO_ERR || !public_id) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(public_id));
	dom_string_unref(public_id);
}

static void
mjs_doctype_get_property_systemId(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_document_type *dtd = (dom_document_type *)(js_touserdata(J, 0, "doctype"));

	if (!dtd) {
		js_pushnull(J);
		return;
	}
	dom_string *system_id = NULL;
	dom_exception exc = dom_document_type_get_system_id(dtd, &system_id);

	if (exc != DOM_NO_ERR || !system_id) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(system_id));
	dom_string_unref(system_id);
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
		addmethod(J, "addEventListener", mjs_document_addEventListener, 3);
		addmethod(J, "createComment",	mjs_document_createComment, 1);
		addmethod(J, "createDocumentFragment",mjs_document_createDocumentFragment, 0);
		addmethod(J, "createElement",	mjs_document_createElement, 1);
		addmethod(J, "createTextNode",	mjs_document_createTextNode, 1);
		addmethod(J, "dispatchEvent",	mjs_document_dispatchEvent, 1);
		addmethod(J, "write",		mjs_document_write, 1);
		addmethod(J, "writeln",		mjs_document_writeln, 1);
		addmethod(J, "replace",		mjs_document_replace, 2);
		addmethod(J, "getElementById",	mjs_document_getElementById, 1);
		addmethod(J, "getElementsByClassName",	mjs_document_getElementsByClassName, 1);
//		addmethod(J, "getElementsByName",	mjs_document_getElementsByName, 1);
		addmethod(J, "getElementsByTagName",	mjs_document_getElementsByTagName, 1);
		addmethod(J, "querySelector",	mjs_document_querySelector, 1);
		addmethod(J, "querySelectorAll",	mjs_document_querySelectorAll, 1);
		addmethod(J, "removeEventListener", mjs_document_removeEventListener, 3);
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
		addproperty(J, "defaultView", mjs_document_get_property_defaultView, NULL);
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
		addproperty(J, "readyState", mjs_document_get_property_readyState, NULL);
		addproperty(J, "scripts", mjs_document_get_property_scripts, NULL);
		addproperty(J, "title",	mjs_document_get_property_title, mjs_document_set_property_title); /* TODO: Charset? */
		addproperty(J, "URL", mjs_document_get_property_url, mjs_document_set_property_url);
	}
	js_defglobal(J, "document", JS_DONTENUM);
//	js_dostring(J, "document.location = location; window.document = document;");

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

void *map_doctypes;

static void
mjs_doctype_finalizer(js_State *J, void *node)
{
	attr_erase_from_map(map_doctypes, node);
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
	attr_save_in_map(map_doctypes, node, node);
}

static void
mjs_doc_private_finalizer(js_State *J, void *priv)
{
	struct mjs_document_private *doc_private = (struct mjs_document_private *)priv;

	if (doc_private) {
		struct el_listener *l;

		if (doc_private->listener) {
			dom_event_listener_unref(doc_private->listener);
		}

		foreach(l, doc_private->listeners) {
			mem_free_set(&l->typ, NULL);
			if (l->fun) js_unref(J, l->fun);
		}
		free_list(doc_private->listeners);
		if (doc_private->node) {
			dom_node_unref(doc_private->node);
		}
		if (doc_private->thisval) {
			js_unref(J, doc_private->thisval);
		}
		mem_free(doc_private);
	}
}

void
mjs_push_document(js_State *J, void *doc)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_document_private *doc_private = (struct mjs_document_private *)mem_calloc(1, sizeof(*doc_private));

	if (!doc_private) {
		return;
	}
	js_newobject(J);
	{
		js_newuserdata(J, "document", doc_private, mjs_doc_private_finalizer);
		addmethod(J, "addEventListener", mjs_document_addEventListener, 3);
		addmethod(J, "createComment",	mjs_document_createComment, 1);
		addmethod(J, "createDocumentFragment",mjs_document_createDocumentFragment, 0);
		addmethod(J, "createElement",	mjs_document_createElement, 1);
		addmethod(J, "createTextNode",	mjs_document_createTextNode, 1);
		addmethod(J, "dispatchEvent",	mjs_document_dispatchEvent, 1);
		addmethod(J, "write",		mjs_document_write, 1);
		addmethod(J, "writeln",		mjs_document_writeln, 1);
		addmethod(J, "replace",		mjs_document_replace, 2);
		addmethod(J, "getElementById",	mjs_document_getElementById, 1);
		addmethod(J, "getElementsByClassName",	mjs_document_getElementsByClassName, 1);
//		addmethod(J, "getElementsByName",	mjs_document_getElementsByName, 1);
		addmethod(J, "getElementsByTagName",	mjs_document_getElementsByTagName, 1);
		addmethod(J, "querySelector",	mjs_document_querySelector, 1);
		addmethod(J, "querySelectorAll",	mjs_document_querySelectorAll, 1);
		addmethod(J, "removeEventListener", mjs_document_removeEventListener, 3);
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
		addproperty(J, "defaultView", mjs_document_get_property_defaultView, NULL);
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
		addproperty(J, "readyState", mjs_document_get_property_readyState, NULL);
		addproperty(J, "referrer", mjs_document_get_property_referrer, NULL);
		addproperty(J, "scripts", mjs_document_get_property_scripts, NULL);
		addproperty(J, "title",	mjs_document_get_property_title, mjs_document_set_property_title); /* TODO: Charset? */
		addproperty(J, "URL", mjs_document_get_property_url, mjs_document_set_property_url);
	}
	js_defglobal(J, "document", JS_DONTENUM);

	init_list(doc_private->listeners);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	doc_private->interpreter = interpreter;
	doc_private->node = doc;
	doc_private->ref_count = 1;
	doc_private->thisval = js_ref(J);
	if (doc) {
		dom_node_ref((dom_node *)doc);
	}
}

static void
mjs_document_dispatchEvent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_document *doc = (dom_document *)mjs_doc_getprivate(J, 0);

	if (!doc) {
		js_pushboolean(J, 0);
		return;
	}
	dom_event *event = (dom_event *)js_touserdata(J, 1, "event");

	if (!event) {
		js_pushboolean(J, 0);
		return;
	}
	bool result = false;
	dom_exception exc = dom_event_target_dispatch_event(doc, event, &result);
	js_pushboolean(J, result);
}

static void
document_event_handler(dom_event *event, void *pw)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_document_private *doc_private = (struct mjs_document_private *)pw;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)doc_private->interpreter;
	js_State *J = (js_State *)interpreter->backend_data;
	dom_document *doc = (dom_document *)doc_private->node;

	if (!event) {
		return;
	}

	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		return;
	}

	if (!strcmp("DOMContentLoaded", dom_string_data(typ))) {
		doc_private->state = COMPLETE;
	}
//	interpreter->heartbeat = add_heartbeat(interpreter);

	struct el_listener *l, *next;

	foreachsafe(l, next, doc_private->listeners) {
		if (strcmp(l->typ, dom_string_data(typ))) {
			continue;
		}
		js_getregistry(J, l->fun); /* retrieve the js function from the registry */
		js_getregistry(J, doc_private->thisval);
		mjs_push_event(J, event);
		js_pcall(J, 1);
		js_pop(J, 1);
	}
//	done_heartbeat(interpreter->heartbeat);
	check_for_rerender(interpreter, dom_string_data(typ));
	dom_string_unref(typ);
}
