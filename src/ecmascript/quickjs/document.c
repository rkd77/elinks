/* The QuickJS document object implementation. */

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

#include "cookies/cookies.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/libdom/doc.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/libdom/parse.h"
#include "ecmascript/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/collection.h"
#include "ecmascript/quickjs/event.h"
#include "ecmascript/quickjs/form.h"
#include "ecmascript/quickjs/forms.h"
#include "ecmascript/quickjs/implementation.h"
#include "ecmascript/quickjs/location.h"
#include "ecmascript/quickjs/document.h"
#include "ecmascript/quickjs/element.h"
#include "ecmascript/quickjs/nodelist.h"
#include "ecmascript/quickjs/window.h"
#include "session/session.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSValue getDoctype(JSContext *ctx, void *node);
static JSClassID js_doctype_class_id;
static JSClassID js_document_class_id;

struct document_listener {
	LIST_HEAD_EL(struct document_listener);
	char *typ;
	JSValue fun;
};

enum readyState {
	LOADING = 0,
	INTERACTIVE,
	COMPLETE
};

struct js_document_private {
	LIST_OF(struct document_listener) listeners;
	struct ecmascript_interpreter *interpreter;
	JSValue thisval;
	dom_event_listener *listener;
	void *node;
	enum readyState state;
};

static void document_event_handler(dom_event *event, void *pw);

void *
js_doc_getopaque(JSValueConst obj, JSClassID class_id)
{
	REF_JS(obj);

	struct js_document_private *res = (struct js_document_private *)JS_GetOpaque(obj, class_id);

	if (!res) {
		return NULL;
	}

	return res->node;
}

static JSValue
js_document_get_property_anchors(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_html_collection *anchors = NULL;
	dom_exception exc = dom_html_document_get_anchors(doc, &anchors);

	if (exc != DOM_NO_ERR || !anchors) {
		return JS_NULL;
	}
	JSValue rr = getCollection(ctx, anchors);
	JS_FreeValue(ctx, rr);

	RETURN_JS(rr);
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
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_html_element *body = NULL;
	dom_exception exc = dom_html_document_get_body(doc, &body);

	if (exc != DOM_NO_ERR || !body) {
		return JS_NULL;
	}

	return getElement(ctx, body);
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
// TODO
	JSValue r = JS_NewStringLen(ctx, "utf-8", strlen("utf-8"));

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
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_element *root = NULL;
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR || !root) {
		return JS_NULL;
	}
	dom_nodelist *nodes = NULL;
	exc = dom_node_get_child_nodes(root, &nodes);
	dom_node_unref(root);

	if (exc != DOM_NO_ERR || !nodes) {
		return JS_NULL;
	}

	return getNodeList(ctx, nodes);
}

static JSValue
js_document_get_property_defaultView(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	JSValue global_obj = JS_GetGlobalObject(ctx);

	RETURN_JS(global_obj);
}

static JSValue
js_document_get_property_doctype(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_document *doc = (struct dom_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_document_type *dtd;
	dom_document_get_doctype(doc, &dtd);

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
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_html_element *root = NULL;
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR || !root) {
		return JS_NULL;
	}

	return getElement(ctx, root);
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
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_html_collection *forms = NULL;
	dom_exception exc = dom_html_document_get_forms(doc, &forms);

	if (exc != DOM_NO_ERR || !forms) {
		return JS_NULL;
	}
	JSValue rr = getForms(ctx, forms);
	JS_FreeValue(ctx, rr);

	RETURN_JS(rr);
}

static JSValue
js_document_get_property_head(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
// TODO
	return JS_NULL;

//	return getElement(ctx, element);
}

static JSValue
js_document_get_property_images(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_html_collection *images = NULL;
	dom_exception exc = dom_html_document_get_images(doc, &images);

	if (exc != DOM_NO_ERR || !images) {
		return JS_NULL;
	}
	JSValue rr = getCollection(ctx, images);
	JS_FreeValue(ctx, rr);

	RETURN_JS(rr);
}

static JSValue
js_document_get_property_implementation(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

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
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_html_collection *links = NULL;
	dom_exception exc = dom_html_document_get_links(doc, &links);

	if (exc != DOM_NO_ERR || !links) {
		return JS_NULL;
	}
	JSValue rr = getCollection(ctx, links);
	JS_FreeValue(ctx, rr);

	RETURN_JS(rr);
}

static JSValue
js_document_get_property_location(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	JSValue ret = getLocation(ctx);

	RETURN_JS(ret);
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
js_document_get_property_readyState(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct js_document_private *doc_private = (struct js_document_private *)(JS_GetOpaque(this_val, js_document_class_id));

	if (!doc_private) {
		return JS_NULL;
	}
	JSValue r;

	switch (doc_private->state) {
	case LOADING:
		r = JS_NewString(ctx, "loading");
		break;
	case INTERACTIVE:
		r = JS_NewString(ctx, "interactive");
		break;
	case COMPLETE:
		r = JS_NewString(ctx, "complete");
		break;
	}
	RETURN_JS(r);
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
	char *str;
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
			str = get_uri_string(ses->referrer, URI_HTTP_REFERRER);

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
		str = get_uri_string(document->uri, URI_HTTP_REFERRER);

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
// TODO
	//dom_html_document *doc = (dom_html_document *)document->dom;

	return JS_NULL;

//	JSValue rr = getCollection(ctx, elements);
//	JS_FreeValue(ctx, rr);

//	RETURN_JS(rr);
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
			interpreter->changed = 1;
			interpreter->was_write = 1;
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

	return JS_TRUE;
}

static JSValue
js_document_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct js_document_private *doc_private = (struct js_document_private *)(JS_GetOpaque(this_val, js_document_class_id));

	if (!doc_private) {
		return JS_NULL;
	}
	dom_document *doc = doc_private->node;

	if (argc < 2) {
		return JS_UNDEFINED;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *method = stracpy(str);
	JS_FreeCString(ctx, str);

	if (!method) {
		return JS_EXCEPTION;
	}
	JSValue fun = argv[1];
	struct document_listener *l;

	foreach(l, doc_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}

		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			mem_free(method);
			return JS_UNDEFINED;
		}
	}
	struct document_listener *n = (struct document_listener *)mem_calloc(1, sizeof(*n));

	if (!n) {
		return JS_UNDEFINED;
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
			return JS_UNDEFINED;
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

	return JS_UNDEFINED;
}

static JSValue
js_document_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct js_document_private *doc_private = (struct js_document_private *)(JS_GetOpaque(this_val, js_document_class_id));

	if (!doc_private) {
		return JS_FALSE;
	}
	dom_document *doc = doc_private->node;

	if (!doc) {
		return JS_FALSE;
	}

	if (argc < 1) {
		return JS_FALSE;
	}
	JSValue eve = argv[0];
	dom_event *event = (dom_event *)(JS_GetOpaque(eve, js_event_class_id));
	bool result = false;
	dom_exception exc = dom_event_target_dispatch_event(doc, event, &result);

	return JS_NewBool(ctx, result);
}

static JSValue
js_document_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct js_document_private *doc_private = (struct js_document_private *)(JS_GetOpaque(this_val, js_document_class_id));

	if (!doc_private) {
		return JS_NULL;
	}
	dom_document *doc = (dom_document *)doc_private->node;

	if (argc < 2) {
		return JS_UNDEFINED;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *method = stracpy(str);
	JS_FreeCString(ctx, str);

	if (!method) {
		return JS_EXCEPTION;
	}
	JSValue fun = argv[1];
	struct document_listener *l;

	foreach(l, doc_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}

		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			dom_string *typ = NULL;
			dom_exception exc = dom_string_create(method, strlen(method), &typ);

			if (exc != DOM_NO_ERR || !typ) {
				continue;
			}
			//dom_event_target_remove_event_listener(doc, typ, doc_private->listener, false);
			dom_string_unref(typ);

			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			mem_free(l);
			mem_free(method);

			return JS_UNDEFINED;
		}
	}
	mem_free(method);

	return JS_UNDEFINED;
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_string *data = NULL;
	dom_exception exc;
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &data);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !data) {
		return JS_NULL;
	}
	dom_comment *comment = NULL;
	exc = dom_document_create_comment(doc, data, &comment);
	dom_string_unref(data);

	if (exc != DOM_NO_ERR || !comment) {
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
	dom_document *doc = (struct dom_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_document_fragment *fragment = NULL;
	dom_exception exc = dom_document_create_document_fragment(doc, &fragment);

	if (exc != DOM_NO_ERR || !fragment) {
		return JS_NULL;
	}

	return getElement(ctx, fragment);
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_document *doc = (struct dom_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_string *tag_name = NULL;
	dom_exception exc;
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &tag_name);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !tag_name) {
		return JS_NULL;
	}
	dom_element *element = NULL;
	exc = dom_document_create_element(doc, tag_name, &element);
	dom_string_unref(tag_name);

	if (exc != DOM_NO_ERR || !element) {
		return JS_NULL;
	}

	return getElement(ctx, element);
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_document *doc = (struct dom_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_string *data = NULL;
	dom_exception exc;
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &data);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !data) {
		return JS_NULL;
	}
	dom_text *text_node = NULL;
	exc = dom_document_create_text_node(doc, data, &text_node);
	dom_string_unref(data);

	if (exc != DOM_NO_ERR || !text_node) {
		return JS_NULL;
	}

	return getElement(ctx, text_node);
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
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_string *id = NULL;
	dom_exception exc;
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &id);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !id) {
		return JS_NULL;
	}
	dom_element *element = NULL;
	exc = dom_document_get_element_by_id(doc, id, &element);
	dom_string_unref(id);

	if (exc != DOM_NO_ERR || !element) {
		return JS_NULL;
	}

	return getElement(ctx, element);
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

// TODO
	return JS_NULL;
#if 0

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
	JSValue rr = getCollection(ctx, elements);
	JS_FreeValue(ctx, rr);

	RETURN_JS(rr);
#endif
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

// TODO
	return JS_NULL;
#if 0
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
	JSValue rr = getCollection(ctx, elements);
	JS_FreeValue(ctx, rr);

	RETURN_JS(rr);
#endif
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
	dom_document *doc = (struct dom_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_string *tagname = NULL;
	dom_exception exc;
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &tagname);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !tagname) {
		return JS_NULL;
	}
	dom_nodelist *nodes = NULL;
	exc = dom_document_get_elements_by_tag_name(doc, tagname, &nodes);
	dom_string_unref(tagname);

	if (exc != DOM_NO_ERR || !nodes) {
		return JS_NULL;
	}
	JSValue rr = getNodeList(ctx, nodes);

	RETURN_JS(rr);
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
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_node *root = NULL; /* root element of document */
	/* Get root element */
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR) {
		return JS_NULL;
	}
	size_t len;
	const char *selector = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!selector) {
		dom_node_unref(root);
		return JS_NULL;
	}
	void *ret = walk_tree_query(root, selector, 0);
	JS_FreeCString(ctx, selector);
	dom_node_unref(root);

	if (!ret) {
		return JS_NULL;
	}
	return getElement(ctx, ret);
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
	dom_html_document *doc = (struct dom_html_document *)js_doc_getopaque(this_val, js_document_class_id);

	if (!doc) {
		return JS_NULL;
	}
	dom_node *doc_root = NULL; /* root element of document */
	/* Get root element */
	dom_exception exc = dom_document_get_document_element(doc, &doc_root);

	if (exc != DOM_NO_ERR) {
		return JS_NULL;
	}
	size_t len;
	const char *selector = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!selector) {
		dom_node_unref(doc_root);
		return JS_NULL;
	}

	dom_string *tag_name = NULL;
	exc = dom_string_create((const uint8_t *)"B", 1, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		dom_node_unref(doc_root);
		JS_FreeCString(ctx, selector);
		return JS_NULL;
	}
	dom_element *element = NULL;
	exc = dom_document_create_element(doc, tag_name, &element);
	dom_string_unref(tag_name);

	if (exc != DOM_NO_ERR || !element) {
		dom_node_unref(doc_root);
		JS_FreeCString(ctx, selector);
		return JS_NULL;
	}
	walk_tree_query_append((dom_node *)element, doc_root, selector, 0);
	dom_node_unref(doc_root);
	JS_FreeCString(ctx, selector);

	dom_nodelist *nodes = NULL;
	exc = dom_node_get_child_nodes(element, &nodes);
	dom_node_unref(element);

	if (exc != DOM_NO_ERR || !nodes) {
		return JS_NULL;
	}
	return getNodeList(ctx, nodes);
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

	dom_document_type *dtd = (dom_document_type *)(JS_GetOpaque(this_val, js_doctype_class_id));

	if (!dtd) {
		return JS_NULL;
	}
	dom_string *name = NULL;
	dom_exception exc = dom_document_type_get_name(dtd, &name);

	if (exc != DOM_NO_ERR || !name) {
		return JS_NULL;
	}
	JSValue ret = JS_NewStringLen(ctx, dom_string_data(name), dom_string_length(name));
	dom_string_unref(name);

	return ret;
}

static JSValue
js_doctype_get_property_publicId(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_document_type *dtd = (dom_document_type *)(JS_GetOpaque(this_val, js_doctype_class_id));

	if (!dtd) {
		return JS_NULL;
	}
	dom_string *public_id = NULL;
	dom_exception exc = dom_document_type_get_public_id(dtd, &public_id);

	if (exc != DOM_NO_ERR || !public_id) {
		return JS_NULL;
	}
	JSValue ret = JS_NewStringLen(ctx, dom_string_data(public_id), dom_string_length(public_id));
	dom_string_unref(public_id);

	return ret;
}

static JSValue
js_doctype_get_property_systemId(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_document_type *dtd = (dom_document_type *)(JS_GetOpaque(this_val, js_doctype_class_id));

	if (!dtd) {
		return JS_NULL;
	}
	dom_string *system_id = NULL;
	dom_exception exc = dom_document_type_get_system_id(dtd, &system_id);

	if (exc != DOM_NO_ERR || !system_id) {
		return JS_NULL;
	}
	JSValue ret = JS_NewStringLen(ctx, dom_string_data(system_id), dom_string_length(system_id));
	dom_string_unref(system_id);

	return ret;
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
	JS_CGETSET_DEF("anchors", js_document_get_property_anchors, NULL),
	JS_CGETSET_DEF("baseURI", js_document_get_property_baseURI, NULL),
	JS_CGETSET_DEF("body", js_document_get_property_body, js_document_set_property_body),
#ifdef CONFIG_COOKIES
	JS_CGETSET_DEF("cookie", js_document_get_property_cookie, js_document_set_property_cookie),
#endif
	JS_CGETSET_DEF("charset", js_document_get_property_charset, NULL),
	JS_CGETSET_DEF("characterSet", js_document_get_property_charset, NULL),
	JS_CGETSET_DEF("childNodes", js_document_get_property_childNodes, NULL),
	JS_CGETSET_DEF("defaultView", js_document_get_property_defaultView, NULL),
	JS_CGETSET_DEF("doctype", js_document_get_property_doctype, NULL),
	JS_CGETSET_DEF("documentElement", js_document_get_property_documentElement, NULL),
	JS_CGETSET_DEF("documentURI", js_document_get_property_documentURI, NULL),
	JS_CGETSET_DEF("domain", js_document_get_property_domain, NULL),
	JS_CGETSET_DEF("forms", js_document_get_property_forms, NULL),
	JS_CGETSET_DEF("head", js_document_get_property_head, NULL),
	JS_CGETSET_DEF("images", js_document_get_property_images, NULL),
	JS_CGETSET_DEF("implementation", js_document_get_property_implementation, NULL),
	JS_CGETSET_DEF("inputEncoding", js_document_get_property_charset, NULL),
	JS_CGETSET_DEF("links", js_document_get_property_links, NULL),
	JS_CGETSET_DEF("location",	js_document_get_property_location, js_document_set_property_location),
	JS_CGETSET_DEF("nodeType", js_document_get_property_nodeType, NULL),
	JS_CGETSET_DEF("readyState", js_document_get_property_readyState, NULL),
	JS_CGETSET_DEF("referrer", js_document_get_property_referrer, NULL),
	JS_CGETSET_DEF("scripts", js_document_get_property_scripts, NULL),
	JS_CGETSET_DEF("title",	js_document_get_property_title, js_document_set_property_title), /* TODO: Charset? */
	JS_CGETSET_DEF("URL", js_document_get_property_url, js_document_set_property_url),

	JS_CFUNC_DEF("addEventListener",	3, js_document_addEventListener),
	JS_CFUNC_DEF("createComment",	1, js_document_createComment),
	JS_CFUNC_DEF("createDocumentFragment",	0, js_document_createDocumentFragment),
	JS_CFUNC_DEF("createElement",	1, js_document_createElement),
	JS_CFUNC_DEF("createTextNode",	1, js_document_createTextNode),
	JS_CFUNC_DEF("dispatchEvent",	1, js_document_dispatchEvent),
	JS_CFUNC_DEF("write",		1, js_document_write),
	JS_CFUNC_DEF("writeln",		1, js_document_writeln),
	JS_CFUNC_DEF("removeEventListener",	3, js_document_removeEventListener),
	JS_CFUNC_DEF("replace",		2, js_document_replace),
	JS_CFUNC_DEF("getElementById",	1, js_document_getElementById),
//	JS_CFUNC_DEF("getElementsByClassName",	1, js_document_getElementsByClassName),
//	JS_CFUNC_DEF("getElementsByName",	1, js_document_getElementsByName),
	JS_CFUNC_DEF("getElementsByTagName",	1, js_document_getElementsByTagName),
	JS_CFUNC_DEF("querySelector",	1, js_document_querySelector),
	JS_CFUNC_DEF("querySelectorAll",	1, js_document_querySelectorAll),

	JS_CFUNC_DEF("toString", 0, js_document_toString)
};

static void
js_document_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	struct js_document_private *doc_private = (struct js_document_private *)JS_GetOpaque(val, js_document_class_id);

	if (doc_private) {
		struct document_listener *l;

		if (doc_private->listener) {
			dom_event_listener_unref(doc_private->listener);
		}

		foreach(l, doc_private->listeners) {
			mem_free_set(&l->typ, NULL);
		}
		free_list(doc_private->listeners);
		if (doc_private->node) {
			dom_node_unref((dom_node *)doc_private->node);
		}
		mem_free(doc_private);
	}
}

static JSClassDef js_document_class = {
	"document",
	js_document_finalizer
};

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
	JS_CGETSET_DEF("name", js_doctype_get_property_name, NULL),
	JS_CGETSET_DEF("publicId", js_doctype_get_property_publicId, NULL),
	JS_CGETSET_DEF("systemId", js_doctype_get_property_systemId, NULL),
	JS_CFUNC_DEF("toString", 0, js_doctype_toString)
};

void *map_doctypes;
//static std::map<void *, JSValueConst> map_doctypes;

static void
js_doctype_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	dom_node *node = (dom_node *)JS_GetOpaque(val, js_doctype_class_id);
	attr_erase_from_map(map_doctypes, node);

	if (node) {
		dom_node_unref(node);
	}
}

static JSClassDef js_doctype_class = {
	"doctype",
	js_doctype_finalizer
};

int
js_doctype_init(JSContext *ctx)
{
	JSValue doctype_proto;

	/* create the doctype class */
	JS_NewClassID(&js_doctype_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_doctype_class_id, &js_doctype_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	doctype_proto = JS_NewObject(ctx);
	REF_JS(doctype_proto);

	JS_SetPropertyFunctionList(ctx, doctype_proto, js_doctype_proto_funcs, countof(js_doctype_proto_funcs));
	JS_SetClassProto(ctx, js_doctype_class_id, doctype_proto);
	JS_SetPropertyStr(ctx, global_obj, "doctype", JS_DupValue(ctx, doctype_proto));

	JS_FreeValue(ctx, global_obj);

	return 0;
}

static JSValue
getDoctype(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue second;
	static int initialized;
	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_doctype_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_doctype_class_id, &js_doctype_class);
		initialized = 1;
	}
	second = attr_find_in_map(map_doctypes, node);

	if (!JS_IsNull(second)) {
		JSValue r = JS_DupValue(ctx, second);
		RETURN_JS(r);
	}
	JSValue doctype_obj = JS_NewObjectClass(ctx, js_doctype_class_id);
	JS_SetPropertyFunctionList(ctx, doctype_obj, js_doctype_proto_funcs, countof(js_doctype_proto_funcs));
	JS_SetClassProto(ctx, js_doctype_class_id, doctype_obj);
	JS_SetOpaque(doctype_obj, node);
	attr_save_in_map(map_doctypes, node, doctype_obj);

	JSValue rr = JS_DupValue(ctx, doctype_obj);
	RETURN_JS(rr);
}

JSValue
getDocument(JSContext *ctx, void *doc)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_document_private *doc_private = (struct js_document_private *)mem_calloc(1, sizeof(*doc_private));

	if (!doc_private) {
		return JS_NULL;
	}
	init_list(doc_private->listeners);
	doc_private->node = doc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	doc_private->interpreter = interpreter;
	static int initialized;

	JSValue global_obj = JS_GetGlobalObject(ctx);

	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_document_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_document_class_id, &js_document_class);
		initialized = 1;
	}
	//REF_JS(global_obj);

	JSValue document_obj = JS_NewObjectClass(ctx, js_document_class_id);
	JS_SetPropertyFunctionList(ctx, document_obj, js_document_proto_funcs, countof(js_document_proto_funcs));
//	document_class = JS_NewCFunction2(ctx, js_document_ctor, "document", 0, JS_CFUNC_constructor, 0);
//	JS_SetConstructor(ctx, document_class, document_obj);
	JS_SetOpaque(document_obj, doc_private);
//	JS_SetClassProto(ctx, js_document_class_id, document_obj);
	JS_SetPropertyStr(ctx, global_obj, "document", document_obj);
	JS_FreeValue(ctx, global_obj);
	doc_private->thisval = document_obj;

	RETURN_JS(document_obj);
}

static void
document_event_handler(dom_event *event, void *pw)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_document_private *doc_private = (struct js_document_private *)pw;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)doc_private->interpreter;
	JSContext *ctx = (JSContext *)interpreter->backend_data;
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

	struct document_listener *l, *next;

	foreachsafe(l, next, doc_private->listeners) {
		if (strcmp(l->typ, dom_string_data(typ))) {
			continue;
		}
		JSValue func = JS_DupValue(ctx, l->fun);
		JSValue arg = getEvent(ctx, event);
		JSValue ret = JS_Call(ctx, func, doc_private->thisval, 1, (JSValueConst *) &arg);
		JS_FreeValue(ctx, ret);
		JS_FreeValue(ctx, func);
		JS_FreeValue(ctx, arg);
	}
//	done_heartbeat(interpreter->heartbeat);
	check_for_rerender(interpreter, dom_string_data(typ));
	dom_string_unref(typ);
}
