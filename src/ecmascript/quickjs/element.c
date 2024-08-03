/* The QuickJS html element objects implementation. */

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

#include "dialogs/status.h"
#include "document/document.h"
#include "document/libdom/corestrings.h"
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#include "document/libdom/renderer2.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/attr.h"
#include "ecmascript/quickjs/attributes.h"
#include "ecmascript/quickjs/collection.h"
#include "ecmascript/quickjs/dataset.h"
#include "ecmascript/quickjs/domrect.h"
#include "ecmascript/quickjs/element.h"
#include "ecmascript/quickjs/event.h"
#include "ecmascript/quickjs/heartbeat.h"
#include "ecmascript/quickjs/keyboard.h"
#include "ecmascript/quickjs/nodelist.h"
#include "ecmascript/quickjs/nodelist2.h"
#include "ecmascript/quickjs/style.h"
#include "ecmascript/quickjs/tokenlist.h"
#include "ecmascript/quickjs/window.h"
#include "session/session.h"
#include "terminal/event.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

JSClassID js_element_class_id;

struct element_listener {
	LIST_HEAD_EL(struct element_listener);
	char *typ;
	JSValue fun;
};

struct js_element_private {
	LIST_OF(struct element_listener) listeners;
	struct ecmascript_interpreter *interpreter;
	JSValue thisval;
	dom_event_listener *listener;
	void *node;
};

static void element_event_handler(dom_event *event, void *pw);
static JSValue js_element_set_property_textContent(JSContext *ctx, JSValueConst this_val, JSValue val);

void *
js_getopaque(JSValueConst obj, JSClassID class_id)
{
	REF_JS(obj);

	struct js_element_private *res = (struct js_element_private *)JS_GetOpaque(obj, class_id);

	if (!res) {
		return NULL;
	}
#if 0
	void *v = attr_find_in_map_void(map_privates, res->node);

	if (!v) {
		return NULL;
	}
#endif

	return res->node;
}

static JSValue
js_element_get_property_attributes(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_namednodemap *attrs = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_attributes(el, &attrs);

	if (exc != DOM_NO_ERR || !attrs) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	JSValue rr = getAttributes(ctx, attrs);
	JS_FreeValue(ctx, rr);
	//dom_node_unref(el);

	RETURN_JS(rr);
}

static JSValue
js_element_get_property_checked(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	struct el_form_control *fc;
	struct form_state *fs;
	struct link *link;
	int offset, linknum;

	if (!vs) {
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}
	doc = doc_view->document;

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);

	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}

	link = &doc->links[linknum];
	fc = get_link_form_control(link);

	if (!fc) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	fs = find_form_state(doc_view, fc);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		//dom_node_unref(el);
		return JS_NULL; /* detached */
	}
	//dom_node_unref(el);

	return JS_NewBool(ctx, fs->state);
}

static JSValue
js_element_set_property_checked(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	struct el_form_control *fc;
	struct form_state *fs;
	struct link *link;
	int offset, linknum;

	if (!vs) {
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}
	doc = doc_view->document;

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);

	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}

	link = &doc->links[linknum];
	fc = get_link_form_control(link);

	if (!fc) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	fs = find_form_state(doc_view, fc);

	if (!fs) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}

	if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	fs->state = JS_ToBool(ctx, val);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_get_property_children(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_nodelist *nodes = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	JSValue rr = getNodeList(ctx, nodes);
	//dom_node_unref(el);

	RETURN_JS(rr);
}

static JSValue
js_element_get_property_childElementCount(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t res = 0;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	exc = dom_nodelist_get_length(nodes, &res);
	dom_nodelist_unref(nodes);
	//dom_node_unref(el);

	return JS_NewUint32(ctx, res);
}

static JSValue
js_element_get_property_childNodes(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_nodelist *nodes = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	JSValue rr = getNodeList(ctx, nodes);
	//dom_node_unref(el);

	RETURN_JS(rr);
}

static JSValue
js_element_get_property_classList(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_element *el = (dom_element *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	dom_tokenlist *tl = NULL;
	dom_exception exc = dom_tokenlist_create(el, corestring_dom_class, &tl);
	//dom_node_unref(el);

	if (exc != DOM_NO_ERR || !tl) {
		return JS_NULL;
	}

	return getTokenlist(ctx, tl);
}

static JSValue
js_element_get_property_className(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	JSValue r;

	dom_html_element *el = (dom_html_element *)(js_getopaque(this_val, js_element_class_id));
	dom_string *classstr = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);

	exc = dom_html_element_get_class_name(el, &classstr);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	if (!classstr) {
		r = JS_NewString(ctx, "");
	} else {
		r = JS_NewStringLen(ctx, dom_string_data(classstr), dom_string_length(classstr));
		dom_string_unref(classstr);
	}
	//dom_node_unref(el);

	RETURN_JS(r);
}

#if 0
static JSValue
js_element_get_property_clientHeight(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		return JS_NewInt32(ctx, 0);
	}
	ses = doc_view->session;

	if (!ses) {
		return JS_NewInt32(ctx, 0);
	}
	dom_string *tag_name = NULL;
	dom_exception exc = dom_node_get_node_name(el, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		return JS_NewInt32(ctx, 0);
	}
	bool root = (!strcmp(dom_string_data(tag_name), "BODY") || !strcmp(dom_string_data(tag_name), "HTML"));
	dom_string_unref(tag_name);

	if (root) {
		int height = doc_view->box.height * ses->tab->term->cell_height;
		return JS_NewInt32(ctx, height);
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		return JS_NewInt32(ctx, 0);
	}
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		return JS_NewInt32(ctx, 0);
	}
	int dy = int_max(0, (rect->y1 + 1 - rect->y0) * ses->tab->term->cell_height);

	return JS_NewInt32(ctx, dy);
}
#endif

#if 0
static JSValue
js_element_get_property_clientLeft(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return JS_NewInt32(ctx, 0);
}
#endif

#if 0
static JSValue
js_element_get_property_clientTop(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return JS_NewInt32(ctx, 0);
}
#endif

#if 0
static JSValue
js_element_get_property_clientWidth(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		return JS_NewInt32(ctx, 0);
	}
	ses = doc_view->session;

	if (!ses) {
		return JS_NewInt32(ctx, 0);
	}
	dom_string *tag_name = NULL;
	dom_exception exc = dom_node_get_node_name(el, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		return JS_NewInt32(ctx, 0);
	}
	bool root = (!strcmp(dom_string_data(tag_name), "BODY") || !strcmp(dom_string_data(tag_name), "HTML") || !strcmp(dom_string_data(tag_name), "DIV"));
	dom_string_unref(tag_name);

	if (root) {
		int width = doc_view->box.width * ses->tab->term->cell_width;
		return JS_NewInt32(ctx, width);
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		return JS_NewInt32(ctx, 0);
	}
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		return JS_NewInt32(ctx, 0);
	}
	int dx = int_max(0, (rect->x1 + 1 - rect->x0) * ses->tab->term->cell_width);

	return JS_NewInt32(ctx, dx);
}
#endif

static JSValue
js_element_get_property_dataset(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_element *el = (dom_element *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	return getDataset(ctx, el);
}

static JSValue
js_element_get_property_dir(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	JSValue r;

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_string *dir = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_element_get_attribute(el, corestring_dom_dir, &dir);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	if (!dir) {
		r = JS_NewString(ctx, "");
	} else {
		if (strcmp(dom_string_data(dir), "auto") && strcmp(dom_string_data(dir), "ltr") && strcmp(dom_string_data(dir), "rtl")) {
			r = JS_NewString(ctx, "");
		} else {
			r = JS_NewStringLen(ctx, dom_string_data(dir), dom_string_length(dir));
		}
		dom_string_unref(dir);
	}
	//dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_element_get_property_firstChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_node *node = NULL;
	dom_exception exc;

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_first_child(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	JSValue rr = getElement(ctx, node);
	dom_node_unref(node);

	return rr;
}

static JSValue
js_element_get_property_firstElementChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t size = 0;
	uint32_t i;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	exc = dom_nodelist_get_length(nodes, &size);

	if (exc != DOM_NO_ERR || !size) {
		dom_nodelist_unref(nodes);
		//dom_node_unref(el);
		return JS_NULL;
	}

	for (i = 0; i < size; i++) {
		dom_node *child = NULL;
		exc = dom_nodelist_item(nodes, i, &child);
		dom_node_type type;

		if (exc != DOM_NO_ERR || !child) {
			continue;
		}

		exc = dom_node_get_node_type(child, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dom_nodelist_unref(nodes);
			JSValue rr = getElement(ctx, child);
			dom_node_unref(child);
			return rr;
		}
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	//dom_node_unref(el);

	return JS_NULL;
}

static JSValue
js_element_get_property_href(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_string *href = NULL;
	dom_exception exc;
	JSValue r;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_element_get_attribute(el, corestring_dom_href, &href);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	if (!href) {
		r = JS_NewString(ctx, "");
	} else {
		r = JS_NewStringLen(ctx, dom_string_data(href), dom_string_length(href));
		dom_string_unref(href);
	}
	//dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_element_get_property_id(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_string *id = NULL;
	dom_exception exc;
	JSValue r;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_element_get_attribute(el, corestring_dom_id, &id);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	if (!id) {
		r = JS_NewString(ctx, "");
	} else {
		r = JS_NewStringLen(ctx, dom_string_data(id), dom_string_length(id));
		dom_string_unref(id);
	}
	//dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_element_get_property_lang(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_string *lang = NULL;
	dom_exception exc;
	JSValue r;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_element_get_attribute(el, corestring_dom_lang, &lang);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	if (!lang) {
		r = JS_NewString(ctx, "");
	} else {
		r = JS_NewStringLen(ctx, dom_string_data(lang), dom_string_length(lang));
		dom_string_unref(lang);
	}
	//dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_element_get_property_lastChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_node *last_child = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_last_child(el, &last_child);

	if (exc != DOM_NO_ERR || !last_child) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	JSValue rr = getElement(ctx, last_child);
	dom_node_unref(last_child);

	return rr;
}

static JSValue
js_element_get_property_lastElementChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t size = 0;
	int i;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	exc = dom_nodelist_get_length(nodes, &size);

	if (exc != DOM_NO_ERR || !size) {
		dom_nodelist_unref(nodes);
		//dom_node_unref(el);
		return JS_NULL;
	}

	for (i = size - 1; i >= 0 ; i--) {
		dom_node *child = NULL;
		exc = dom_nodelist_item(nodes, i, &child);
		dom_node_type type;

		if (exc != DOM_NO_ERR || !child) {
			continue;
		}
		exc = dom_node_get_node_type(child, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dom_nodelist_unref(nodes);
			JSValue rr = getElement(ctx, child);
			dom_node_unref(child);
			return rr;
		}
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	//dom_node_unref(el);

	return JS_NULL;
}

static JSValue
js_element_get_property_nextElementSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_node *node;
	dom_node *prev_next = NULL;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	node = el;

	while (true) {
		dom_node *next = NULL;
		dom_exception exc = dom_node_get_next_sibling(node, &next);
		dom_node_type type;

		if (prev_next) {
			dom_node_unref(prev_next);
		}

		if (exc != DOM_NO_ERR || !next) {
			return JS_NULL;
		}
		exc = dom_node_get_node_type(next, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			JSValue rr = getElement(ctx, next);
			dom_node_unref(next);
			return rr;
		}
		prev_next = next;
		node = next;
	}

	return JS_NULL;
}

static JSValue
js_element_get_property_nodeName(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *node = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_string *name = NULL;
	dom_exception exc;
	JSValue r;

	if (!node) {
		r = JS_NewStringLen(ctx, "", 0);
		RETURN_JS(r);
	}
	//dom_node_ref(node);
	exc = dom_node_get_node_name(node, &name);

	if (exc != DOM_NO_ERR || !name) {
		r = JS_NewStringLen(ctx, "", 0);
		//dom_node_unref(node);

		RETURN_JS(r);
	}
	r = JS_NewStringLen(ctx, dom_string_data(name), dom_string_length(name));
	dom_string_unref(name);
	//dom_node_unref(node);

	RETURN_JS(r);
}

static JSValue
js_element_get_property_nodeType(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *node = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_node_type type;
	dom_exception exc;

	if (!node) {
		return JS_NULL;
	}
	//dom_node_ref(node);
	exc = dom_node_get_node_type(node, &type);

	if (exc == DOM_NO_ERR) {
		//dom_node_unref(node);
		return JS_NewUint32(ctx, type);
	}
	//dom_node_unref(node);

	return JS_NULL;
}

static JSValue
js_element_get_property_nodeValue(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *node = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_string *content = NULL;
	dom_exception exc;
	JSValue r;

	if (!node) {
		return JS_NULL;
	}
	//dom_node_ref(node);
	exc = dom_node_get_node_value(node, &content);

	if (exc != DOM_NO_ERR || !content) {
		//dom_node_unref(node);
		return JS_NULL;
	}
	r = JS_NewStringLen(ctx, dom_string_data(content), dom_string_length(content));
	dom_string_unref(content);
	//dom_node_unref(node);

	RETURN_JS(r);
}

static JSValue
js_element_get_property_nextSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_next_sibling(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	//dom_node_unref(el);
	JSValue rr = getElement(ctx, node);
	dom_node_unref(node);

	return rr;
}

#if 0
static JSValue
js_element_get_property_offsetHeight(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_element_get_property_clientHeight(ctx, this_val);
}
#endif

#if 0
static JSValue
js_element_get_property_offsetLeft(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		return JS_NewInt32(ctx, 0);
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		return JS_NewInt32(ctx, 0);
	}
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		return JS_NewInt32(ctx, 0);
	}
	ses = doc_view->session;

	if (!ses) {
		return JS_NewInt32(ctx, 0);
	}
	dom_node *node = NULL;
	dom_exception exc = dom_node_get_parent_node(el, &node);
	if (exc != DOM_NO_ERR || !node) {
		return JS_NewInt32(ctx, 0);
	}
	int offset_parent = find_offset(document->element_map_rev, node);

	if (offset_parent <= 0) {
		return JS_NewInt32(ctx, 0);
	}
	struct node_rect *rect_parent = get_element_rect(document, offset_parent);

	if (!rect_parent) {
		return JS_NewInt32(ctx, 0);
	}
	int dx = int_max(0, (rect->x0 - rect_parent->x0) * ses->tab->term->cell_width);
	dom_node_unref(node);
	return JS_NewInt32(ctx, dx);
}
#endif

static JSValue
js_element_get_property_offsetParent(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_node *el = (dom_node *)js_getopaque(this_val, js_element_class_id);
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	//dom_node_unref(el);

	return getElement(ctx, node);
}

#if 0
static JSValue
js_element_get_property_offsetTop(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		return JS_NewInt32(ctx, 0);
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		return JS_NewInt32(ctx, 0);
	}
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		return JS_NewInt32(ctx, 0);
	}
	ses = doc_view->session;

	if (!ses) {
		return JS_NewInt32(ctx, 0);
	}
	dom_node *node = NULL;
	dom_exception exc = dom_node_get_parent_node(el, &node);
	if (exc != DOM_NO_ERR || !node) {
		return JS_NewInt32(ctx, 0);
	}
	int offset_parent = find_offset(document->element_map_rev, node);

	if (offset_parent <= 0) {
		return JS_NewInt32(ctx, 0);
	}
	struct node_rect *rect_parent = get_element_rect(document, offset_parent);

	if (!rect_parent) {
		return JS_NewInt32(ctx, 0);
	}
	int dy = int_max(0, (rect->y0 - rect_parent->y0) * ses->tab->term->cell_height);
	dom_node_unref(node);
	return JS_NewInt32(ctx, dy);
}
#endif

#if 0
static JSValue
js_element_get_property_offsetWidth(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_element_get_property_clientWidth(ctx, this_val);
}
#endif

static JSValue
js_element_get_property_ownerDocument(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

	JSValue r = JS_DupValue(ctx, interpreter->document_obj);
	RETURN_JS(r);
}

static JSValue
js_element_get_property_parentElement(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)js_getopaque(this_val, js_element_class_id);
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	JSValue rr = getElement(ctx, node);
	dom_node_unref(node);

	return rr;
}

static JSValue
js_element_get_property_parentNode(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_node *el = (dom_node *)js_getopaque(this_val, js_element_class_id);
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	JSValue rr = getElement(ctx, node);
	dom_node_unref(node);

	return rr;
}

static JSValue
js_element_get_property_previousElementSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_node *node;
	dom_node *prev_prev = NULL;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	node = el;

	while (true) {
		dom_node *prev = NULL;
		dom_exception exc = dom_node_get_previous_sibling(node, &prev);
		dom_node_type type;

		if (prev_prev) {
			dom_node_unref(prev_prev);
		}

		if (exc != DOM_NO_ERR || !prev) {
			return JS_NULL;
		}
		exc = dom_node_get_node_type(prev, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			JSValue rr = getElement(ctx, prev);
			dom_node_unref(prev);
			return rr;
		}
		prev_prev = prev;
		node = prev;
	}
	//dom_node_unref(el);

	return JS_NULL;
}

static JSValue
js_element_get_property_previousSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_node_get_previous_sibling(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	//dom_node_unref(el);
	JSValue rr = getElement(ctx, node);
	dom_node_unref(node);

	return rr;
}

static JSValue
js_element_get_property_style(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}

	return getStyle(ctx, el);
}


static JSValue
js_element_get_property_tagName(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	JSValue r;

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	dom_string *tag_name = NULL;
	dom_exception exc = dom_node_get_node_name(el, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	r = JS_NewStringLen(ctx, dom_string_data(tag_name), dom_string_length(tag_name));
	dom_string_unref(tag_name);
	//dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_element_get_property_title(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_string *title = NULL;
	dom_exception exc;
	JSValue r;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	exc = dom_element_get_attribute(el, corestring_dom_title, &title);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	if (!title) {
		r = JS_NewString(ctx, "");
	} else {
		r = JS_NewStringLen(ctx, dom_string_data(title), dom_string_length(title));
		dom_string_unref(title);
	}
	//dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_element_get_property_value(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	JSValue r;
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	struct el_form_control *fc;
	struct form_state *fs;
	struct link *link;
	int offset, linknum;

	if (!vs) {
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}
	doc = doc_view->document;

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}

	link = &doc->links[linknum];
	fc = get_link_form_control(link);

	if (!fc) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	fs = find_form_state(doc_view, fc);

	if (!fs) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	r = JS_NewString(ctx, fs->value);
	//dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_element_get_property_innerHtml(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	struct string buf;

	if (!init_string(&buf)) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	ecmascript_walk_tree(&buf, el, true, false);
	JSValue ret = JS_NewStringLen(ctx, buf.source, buf.length);
	done_string(&buf);
	//dom_node_unref(el);

	RETURN_JS(ret);
}

static JSValue
js_element_get_property_outerHtml(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	struct string buf;

	if (!init_string(&buf)) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	ecmascript_walk_tree(&buf, el, false, false);
	JSValue ret = JS_NewStringLen(ctx, buf.source, buf.length);
	done_string(&buf);
	//dom_node_unref(el);

	RETURN_JS(ret);
}

static JSValue
js_element_get_property_textContent(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	dom_string *content = NULL;
	dom_exception exc = dom_node_get_text_content(el, &content);

	if (exc != DOM_NO_ERR || !content) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	JSValue ret = JS_NewStringLen(ctx, dom_string_data(content), dom_string_length(content));
	dom_string_unref(content);
	//dom_node_unref(el);

	RETURN_JS(ret);
}

static JSValue
js_element_set_property_className(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);
	dom_string *classstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);
	dom_html_element *el = (dom_html_element *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &classstr);

	if (exc == DOM_NO_ERR && classstr) {
		exc = dom_html_element_set_class_name(el, classstr);
		interpreter->changed = 1;
		dom_string_unref(classstr);
	}
	JS_FreeCString(ctx, str);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_dir(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	if (!strcmp(str, "ltr") || !strcmp(str, "rtl") || !strcmp(str, "auto")) {
		dom_string *dir = NULL;
		exc = dom_string_create((const uint8_t *)str, len, &dir);

		if (exc == DOM_NO_ERR && dir) {
			exc = dom_element_set_attribute(el, corestring_dom_dir, dir);
			interpreter->changed = 1;
			dom_string_unref(dir);
		}
	}
	JS_FreeCString(ctx, str);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_href(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);
	dom_string *hrefstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &hrefstr);

	if (exc == DOM_NO_ERR && hrefstr) {
		exc = dom_element_set_attribute(el, corestring_dom_href, hrefstr);
		interpreter->changed = 1;
		dom_string_unref(hrefstr);
	}
	JS_FreeCString(ctx, str);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_id(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);
	dom_string *idstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &idstr);

	if (exc == DOM_NO_ERR && idstr) {
		exc = dom_element_set_attribute(el, corestring_dom_id, idstr);
		interpreter->changed = 1;
		dom_string_unref(idstr);
	}
	JS_FreeCString(ctx, str);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_innerHtml(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);

	size_t size;
	const char *s = JS_ToCStringLen(ctx, &size, val);

	if (!s) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	dom_hubbub_parser *parser = NULL;
	struct dom_document *doc = NULL;
	struct dom_document_fragment *fragment = NULL;
	dom_exception exc;
	struct dom_node *child = NULL, *html = NULL, *body = NULL;
	struct dom_nodelist *bodies = NULL;

	exc = dom_node_get_owner_document(el, &doc);
	if (exc != DOM_NO_ERR) goto out;

	parse_params.enc = "UTF-8";
	parse_params.fix_enc = true;
	parse_params.enable_script = false;
	parse_params.msg = NULL;
	parse_params.script = NULL;
	parse_params.ctx = NULL;
	parse_params.daf = NULL;

	error = dom_hubbub_fragment_parser_create(&parse_params,
						  doc,
						  &parser,
						  &fragment);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to create fragment parser!");
		goto out;
	}

	error = dom_hubbub_parser_parse_chunk(parser, (const uint8_t*)s, size);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to parse HTML chunk");
		goto out;
	}
	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to complete parser");
		goto out;
	}

	/* Parse is finished, transfer contents of fragment into node */

	/* 1. empty this node */
	exc = dom_node_get_first_child(el, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		struct dom_node *cref;
		exc = dom_node_remove_child(el, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(child);
		child = NULL;
		dom_node_unref(cref);
		exc = dom_node_get_first_child(el, &child);
		if (exc != DOM_NO_ERR) goto out;
	}

	/* 2. the first child in the fragment will be an HTML element
	 * because that's how hubbub works, walk through that to the body
	 * element hubbub will have created, we want to migrate that element's
	 * children into ourself.
	 */
	exc = dom_node_get_first_child(fragment, &html);
	if (exc != DOM_NO_ERR) goto out;

	/* We can then ask that HTML element to give us its body */
	exc = dom_element_get_elements_by_tag_name(html, corestring_dom_BODY, &bodies);
	if (exc != DOM_NO_ERR) goto out;

	/* And now we can get the body which will be the zeroth body */
	exc = dom_nodelist_item(bodies, 0, &body);
	if (exc != DOM_NO_ERR) goto out;

	/* 3. Migrate the children */
	exc = dom_node_get_first_child(body, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		struct dom_node *cref;
		exc = dom_node_remove_child(body, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(cref);
		exc = dom_node_append_child(el, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(cref);
		dom_node_unref(child);
		child = NULL;
		exc = dom_node_get_first_child(body, &child);
		if (exc != DOM_NO_ERR) goto out;
	}
out:
	if (parser != NULL) {
		dom_hubbub_parser_destroy(parser);
	}
	if (doc != NULL) {
		dom_node_unref(doc);
	}
	if (fragment != NULL) {
		dom_node_unref(fragment);
	}
	if (child != NULL) {
		dom_node_unref(child);
	}
	if (html != NULL) {
		dom_node_unref(html);
	}
	if (bodies != NULL) {
		dom_nodelist_unref(bodies);
	}
	if (body != NULL) {
		dom_node_unref(body);
	}
	JS_FreeCString(ctx, s);
	interpreter->changed = 1;
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_innerText(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_element_set_property_textContent(ctx, this_val, val);
}

static JSValue
js_element_set_property_lang(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);
	dom_string *langstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);

	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &langstr);

	if (exc == DOM_NO_ERR && langstr) {
		exc = dom_element_set_attribute(el, corestring_dom_lang, langstr);
		interpreter->changed = 1;
		dom_string_unref(langstr);
	}
	JS_FreeCString(ctx, str);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_outerHtml(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	dom_node *parent = NULL;
	dom_exception exc = dom_node_get_parent_node(el, &parent);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	size_t size;
	const char *s = JS_ToCStringLen(ctx, &size, val);

	if (!s) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	struct dom_node *cref = NULL;

	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	dom_hubbub_parser *parser = NULL;
	struct dom_document *doc = NULL;
	struct dom_document_fragment *fragment = NULL;
	struct dom_node *child = NULL, *html = NULL, *body = NULL;
	struct dom_nodelist *bodies = NULL;

	exc = dom_node_get_owner_document(el, &doc);
	if (exc != DOM_NO_ERR) goto out;

	parse_params.enc = "UTF-8";
	parse_params.fix_enc = true;
	parse_params.enable_script = false;
	parse_params.msg = NULL;
	parse_params.script = NULL;
	parse_params.ctx = NULL;
	parse_params.daf = NULL;

	error = dom_hubbub_fragment_parser_create(&parse_params,
						  doc,
						  &parser,
						  &fragment);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to create fragment parser!");
		goto out;
	}

	error = dom_hubbub_parser_parse_chunk(parser, (const uint8_t*)s, size);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to parse HTML chunk");
		goto out;
	}
	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to complete parser");
		goto out;
	}

	/* The first child in the fragment will be an HTML element
	 * because that's how hubbub works, walk through that to the body
	 * element hubbub will have created, we want to migrate that element's
	 * children into ourself.
	 */
	exc = dom_node_get_first_child(fragment, &html);
	if (exc != DOM_NO_ERR) goto out;

	/* We can then ask that HTML element to give us its body */
	exc = dom_element_get_elements_by_tag_name(html, corestring_dom_BODY, &bodies);
	if (exc != DOM_NO_ERR) goto out;

	/* And now we can get the body which will be the zeroth body */
	exc = dom_nodelist_item(bodies, 0, &body);
	if (exc != DOM_NO_ERR) goto out;

	/* Migrate the children */
	exc = dom_node_get_first_child(body, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		exc = dom_node_remove_child(body, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(cref);

		dom_node *spare = NULL;
		exc = dom_node_insert_before(parent, child, el, &spare);

		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(spare);
		dom_node_unref(cref);
		dom_node_unref(child);
		child = NULL;
		exc = dom_node_get_first_child(body, &child);
		if (exc != DOM_NO_ERR) goto out;
	}
	exc = dom_node_remove_child(parent, el, &cref);

	if (exc != DOM_NO_ERR) goto out;
out:
	if (parser != NULL) {
		dom_hubbub_parser_destroy(parser);
	}
	if (doc != NULL) {
		dom_node_unref(doc);
	}
	if (fragment != NULL) {
		dom_node_unref(fragment);
	}
	if (child != NULL) {
		dom_node_unref(child);
	}
	if (html != NULL) {
		dom_node_unref(html);
	}
	if (bodies != NULL) {
		dom_nodelist_unref(bodies);
	}
	if (body != NULL) {
		dom_node_unref(body);
	}
	if (cref != NULL) {
		dom_node_unref(cref);
	}
	dom_node_unref(parent);
	JS_FreeCString(ctx, s);
	interpreter->changed = 1;
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_textContent(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	dom_string *content = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, len, &content);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !content) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_node_set_text_content(el, content);
	dom_string_unref(content);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_title(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);
	dom_string *titlestr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)str, len, &titlestr);

	if (exc == DOM_NO_ERR && titlestr) {
		exc = dom_element_set_attribute(el, corestring_dom_title, titlestr);
		interpreter->changed = 1;
		dom_string_unref(titlestr);
	}
	JS_FreeCString(ctx, str);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_value(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	struct el_form_control *fc;
	struct form_state *fs;
	struct link *link;
	int offset, linknum;

	if (!vs) {
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}
	doc = doc_view->document;

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}

	link = &doc->links[linknum];
	fc = get_link_form_control(link);

	if (!fc) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	fs = find_form_state(doc_view, fc);

	if (!fs) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}

	if (fc->type != FC_FILE) {
		const char *str;
		char *string;
		size_t len;

		str = JS_ToCStringLen(ctx, &len, val);

		if (!str) {
			//dom_node_unref(el);
			return JS_EXCEPTION;
		}

		string = stracpy(str);
		JS_FreeCString(ctx, str);

		mem_free_set(&fs->value, string);

		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD) {
			fs->state = strlen(fs->value);
		}
	}
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

#if 0
// Common part of all add_child_element*() methods.
static xmlpp::Element*
el_add_child_element_common(xmlNode* child, xmlNode* node)
{
	if (!node) {
		xmlFreeNode(child);
		throw xmlpp::internal_error("Could not add child element node");
	}
	xmlpp::Node::create_wrapper(node);

	return static_cast<xmlpp::Element*>(node->_private);
}
#endif

static JSValue
js_element_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct js_element_private *el_private = (struct js_element_private *)(JS_GetOpaque(this_val, js_element_class_id));

	if (!el_private) {
		return JS_FALSE;
	}
	dom_node *el = el_private->node;

	if (!el) {
		return JS_FALSE;
	}
	//dom_node_ref(el);

	if (argc < 1) {
		//dom_node_unref(el);
		return JS_FALSE;
	}
	JSValue eve = argv[0];
//	JS_DupValue(ctx, eve);
	dom_event *event = (dom_event *)(JS_GetOpaque(eve, js_event_class_id));

	if (event) {
		dom_event_ref(event);
	}
	bool result = false;
	(void)dom_event_target_dispatch_event(el, event, &result);

	if (event) {
		dom_event_unref(event);
	}
	//dom_node_unref(el);

	return JS_NewBool(ctx, result);
}

static JSValue
js_element_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct js_element_private *el_private = (struct js_element_private *)(JS_GetOpaque(this_val, js_element_class_id));

	if (!el_private) {
		return JS_NULL;
	}
	dom_node *el = el_private->node;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);

	if (argc < 2) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	char *method = stracpy(str);
	JS_FreeCString(ctx, str);

	if (!method) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	JSValue fun = argv[1];
	struct element_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}

		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			mem_free(method);
			return JS_UNDEFINED;
		}
	}
	struct element_listener *n = (struct element_listener *)mem_calloc(1, sizeof(*n));

	if (!n) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	n->fun = JS_DupValue(ctx, fun);
	n->typ = method;
	add_to_list_end(el_private->listeners, n);
	dom_exception exc;

	if (el_private->listener) {
		dom_event_listener_ref(el_private->listener);
	} else {
		exc = dom_event_listener_create(element_event_handler, el_private, &el_private->listener);

		if (exc != DOM_NO_ERR || !el_private->listener) {
			//dom_node_unref(el);
			return JS_UNDEFINED;
		}
	}
	dom_string *typ = NULL;
	exc = dom_string_create((const uint8_t *)method, strlen(method), &typ);

	if (exc != DOM_NO_ERR || !typ) {
		goto ex;
	}
	exc = dom_event_target_add_event_listener(el, typ, el_private->listener, false);

	if (exc == DOM_NO_ERR) {
		dom_event_listener_ref(el_private->listener);
	}

ex:
	if (typ) {
		dom_string_unref(typ);
	}
	dom_event_listener_unref(el_private->listener);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct js_element_private *el_private = (struct js_element_private *)(JS_GetOpaque(this_val, js_element_class_id));

	if (!el_private) {
		return JS_NULL;
	}
	dom_node *el = el_private->node;

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);

	if (argc < 2) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	char *method = stracpy(str);
	JS_FreeCString(ctx, str);

	if (!method) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	JSValue fun = argv[1];

	struct element_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}

		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			dom_string *typ = NULL;
			dom_exception exc = dom_string_create((const uint8_t *)method, strlen(method), &typ);

			if (exc != DOM_NO_ERR || !typ) {
				continue;
			}
			//dom_event_target_remove_event_listener(el, typ, el_private->listener, false);
			dom_string_unref(typ);

			del_from_list(l);
			JS_FreeValue(ctx, l->fun);
			mem_free_set(&l->typ, NULL);
			mem_free(l);
			mem_free(method);
			//dom_node_unref(el);

			return JS_UNDEFINED;
		}
	}
	mem_free(method);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_appendChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_node *res = NULL;
	dom_exception exc;

	if (argc != 1) {
		return JS_NULL;
	}

	if (!el) {
		return JS_EXCEPTION;
	}
	//dom_node_ref(el);
	dom_node *el2 = (dom_node *)(js_getopaque(argv[0], js_element_class_id));

	if (!el2) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_node_append_child(el, el2, &res);

	if (exc == DOM_NO_ERR && res) {
		interpreter->changed = 1;
		//dom_node_unref(el);
		JSValue rr = getElement(ctx, res);
		dom_node_unref(res);

		return rr;
	}
	//dom_node_unref(el);

	return JS_EXCEPTION;
}

/* @element_funcs{"blur"} */
static JSValue
js_element_blur(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	/* We are a text-mode browser and there *always* has to be something
	 * selected.  So we do nothing for now. (That was easy.) */
	return JS_UNDEFINED;
}

static JSValue
js_element_click(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	struct session *ses;
	int offset, linknum;

	if (!vs) {
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}
	doc = doc_view->document;

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	ses = doc_view->session;
	jump_to_link_number(ses, doc_view, linknum);

	if (enter(ses, doc_view, 0) == FRAME_EVENT_REFRESH) {
		refresh_view(ses, doc_view, 0);
	} else {
		print_screen_status(ses);
	}
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_cloneNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);

	dom_exception exc;
	bool deep = JS_ToBool(ctx, argv[0]);
	dom_node *clone = NULL;
	exc = dom_node_clone_node(el, deep, &clone);

	if (exc != DOM_NO_ERR || !clone) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	//dom_node_unref(el);
	JSValue rr = getElement(ctx, clone);
	dom_node_unref(clone);

	return rr;
}

#if 0
static bool
isAncestor(dom_node *el, dom_node *node)
{
	dom_node *prev_next = NULL;
	while (node) {
		dom_exception exc;
		dom_node *next = NULL;
		if (prev_next) {
			dom_node_unref(prev_next);
		}
		if (el == node) {
			return true;
		}
		exc = dom_node_get_parent_node(node, &next);
		if (exc != DOM_NO_ERR || !next) {
			break;
		}
		prev_next = next;
		node = next;
	}

	return false;
}
#endif

static JSValue
js_element_closest(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	void *res = NULL;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		return JS_NULL;
	}
	size_t len;
	const char *selector = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!selector) {
		return JS_NULL;
	}
	dom_node *root = NULL; /* root element of document */
	/* Get root element */
	dom_exception exc = dom_document_get_document_element(document->dom, &root);

	if (exc != DOM_NO_ERR || !root) {
		JS_FreeCString(ctx, selector);
		return JS_NULL;
	}

	dom_node *el_copy = el;

	if (el_copy) {
		dom_node_ref(el_copy);
	}

	while (el) {
		res = el_match_selector(selector, el);

		if (res) {
			break;
		}
		if (el == root) {
			break;
		}
		dom_node *node = NULL;
		exc = dom_node_get_parent_node(el, &node);
		if (exc != DOM_NO_ERR || !node) {
			break;
		}
		el = node;
	}
	JS_FreeCString(ctx, selector);
	dom_node_unref(root);

	if (el_copy) {
		dom_node_unref(el_copy);
	}

	if (!res) {
		return JS_NULL;
	}
	return getElement(ctx, res);
}

static JSValue
js_element_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_FALSE;
	}
	//dom_node_ref(el);
	dom_node *el2 = (dom_node *)(js_getopaque(argv[0], js_element_class_id));

	if (!el2) {
		//dom_node_unref(el);
		return JS_FALSE;
	}
	dom_node_ref(el2);

	while (1) {
		if (el == el2) {
			//dom_node_unref(el);
			dom_node_unref(el2);
			return JS_TRUE;
		}
		dom_node *node = NULL;
		dom_exception exc = dom_node_get_parent_node(el2, &node);

		if (exc != DOM_NO_ERR || !node) {
			//dom_node_unref(el);
			dom_node_unref(el2);
			return JS_FALSE;
		}
		dom_node_unref(el2);
		el2 = node;
	}
}

static JSValue
js_element_focus(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	int offset, linknum;

	if (!vs) {
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}
	doc = doc_view->document;

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	jump_to_link_number(doc_view->session, doc_view, linknum);
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_getAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_exception exc;
	dom_string *attr_name = NULL;
	dom_string *attr_value = NULL;
	JSValue r;

	if (!el) {
		return JS_FALSE;
	}
	//dom_node_ref(el);
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		//dom_node_unref(el);
		return JS_NULL;
	}

	exc = dom_string_create((const uint8_t *)str, len, &attr_name);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !attr_name) {
		//dom_node_unref(el);
		return JS_NULL;
	}

	exc = dom_element_get_attribute(el, attr_name, &attr_value);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR || !attr_value) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	r = JS_NewStringLen(ctx, dom_string_data(attr_value), dom_string_length(attr_value));
	dom_string_unref(attr_value);
	//dom_node_unref(el);

	RETURN_JS(r);
}

static JSValue
js_element_getAttributeNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_exception exc;
	dom_string *attr_name = NULL;
	dom_attr *attr = NULL;

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	exc = dom_string_create((const uint8_t *)str, len, &attr_name);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !attr_name) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	exc = dom_element_get_attribute_node(el, attr_name, &attr);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR || !attr) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	//dom_node_unref(el);

	return getAttr(ctx, attr);
}

static JSValue
js_element_getBoundingClientRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	JSValue rect = getDomRect(ctx);

	RETURN_JS(rect);
}

static JSValue
js_element_getElementsByTagName(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_FALSE;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	dom_nodelist *nlist = NULL;
	dom_exception exc;
	dom_string *tagname = NULL;

	exc = dom_string_create((const uint8_t *)str, len, &tagname);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !tagname) {
		//dom_node_unref(el);
		return JS_NULL;
	}

	exc = dom_element_get_elements_by_tag_name(el, tagname, &nlist);
	dom_string_unref(tagname);

	if (exc != DOM_NO_ERR || !nlist) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	JSValue rr = getNodeList(ctx, nlist);
	//dom_node_unref(el);

	RETURN_JS(rr);
}

static JSValue
js_element_hasAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_FALSE;
	}
	//dom_node_ref(el);
	size_t slen;
	const char *s = JS_ToCStringLen(ctx, &slen, argv[0]);

	if (!s) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	dom_string *attr_name = NULL;
	dom_exception exc;
	bool res;
	exc = dom_string_create((const uint8_t *)s, slen, &attr_name);
	JS_FreeCString(ctx, s);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_NULL;
	}

	exc = dom_element_has_attribute(el, attr_name, &res);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	//dom_node_unref(el);

	return JS_NewBool(ctx, res);
}

static JSValue
js_element_hasAttributes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 0) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_exception exc;
	bool res;

	if (!el) {
		return JS_FALSE;
	}
	//dom_node_ref(el);
	exc = dom_node_has_attributes(el, &res);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_FALSE;
	}
	//dom_node_unref(el);

	return JS_NewBool(ctx, res);
}

static JSValue
js_element_hasChildNodes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 0) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_exception exc;
	bool res;

	if (!el) {
		return JS_FALSE;
	}
	//dom_node_ref(el);
	exc = dom_node_has_child_nodes(el, &res);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_FALSE;
	}
	//dom_node_unref(el);

	return JS_NewBool(ctx, res);
}

static JSValue
js_element_insertBefore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 2) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);

	JSValue next_sibling1 = argv[1];
	JSValue child1 = argv[0];

	dom_node *next_sibling = (dom_node *)(js_getopaque(next_sibling1, js_element_class_id));

	if (!next_sibling) {
		//dom_node_unref(el);
		return JS_NULL;
	}

	dom_node *child = (dom_node *)(js_getopaque(child1, js_element_class_id));

	dom_exception err;
	dom_node *spare;

	err = dom_node_insert_before(el, child, next_sibling, &spare);
	if (err != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	interpreter->changed = 1;
	//dom_node_unref(el);

	return getElement(ctx, spare);
}

static JSValue
js_element_isEqualNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_FALSE;
	}
	//dom_node_ref(el);

	JSValue node = argv[0];
	dom_node *el2 = (dom_node *)(js_getopaque(node, js_element_class_id));

	struct string first;
	struct string second;

	if (!init_string(&first)) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	if (!init_string(&second)) {
		//dom_node_unref(el);
		done_string(&first);
		return JS_EXCEPTION;
	}

	ecmascript_walk_tree(&first, el, false, true);
	ecmascript_walk_tree(&second, el2, false, true);

	bool ret = !strcmp(first.source, second.source);

	done_string(&first);
	done_string(&second);
	//dom_node_unref(el);

	return JS_NewBool(ctx, ret);
}

static JSValue
js_element_isSameNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_FALSE;
	}
	//dom_node_ref(el);
	JSValue node = argv[0];
	dom_node *el2 = (dom_node *)(js_getopaque(node, js_element_class_id));
	bool res = (el == el2);
	//dom_node_unref(el);

	return JS_NewBool(ctx, res);
}

static JSValue
js_element_matches(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	if (argc != 1) {
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_FALSE;
	}
	//dom_node_ref(el);
	const char *selector;
	size_t len;
	selector = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!selector) {
		//dom_node_unref(el);
		return JS_FALSE;
	}
	void *res = el_match_selector(selector, el);
	JS_FreeCString(ctx, selector);
	//dom_node_unref(el);

	return JS_NewBool(ctx, res != NULL);
}

static JSValue
js_element_querySelector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	size_t len;
	const char *selector = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!selector) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	void *ret = walk_tree_query(el, selector, 0);
	JS_FreeCString(ctx, selector);

	if (!ret) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	//dom_node_unref(el);
	JSValue rr = getElement(ctx, ret);
	dom_node_unref((dom_node *)ret);

	return rr;
}

static JSValue
js_element_querySelectorAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
		return JS_NULL;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	//dom_node_ref(el);

	size_t len;
	const char *selector = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!selector) {
		//dom_node_unref(el);
		return JS_NULL;
	}
	LIST_OF(struct selector_node) *result_list = (LIST_OF(struct selector_node) *)mem_calloc(1, sizeof(*result_list));

	if (!result_list) {
		JS_FreeCString(ctx, selector);
		//dom_node_unref(el);
		return JS_NULL;
	}
	init_list(*result_list);
	walk_tree_query_append(el, selector, 0, result_list);
	JS_FreeCString(ctx, selector);
	//dom_node_unref(el);

	JSValue rr = getNodeList2(ctx, result_list);
	free_list(*result_list);
	mem_free(result_list);

	return rr;
}

static JSValue
js_element_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_node *parent = NULL;
	dom_exception exc;

	if (!el) {
		return JS_UNDEFINED;
	}
	exc = dom_node_get_parent_node(el, &parent);

	if (exc != DOM_NO_ERR || !parent) {
		return JS_UNDEFINED;
	}
	dom_node *res = NULL;
	exc = dom_node_remove_child(parent, el, &res);
	dom_node_unref(parent);

	if (exc == DOM_NO_ERR) {
		dom_node_unref(res);
		interpreter->changed = 1;
	}

	return JS_UNDEFINED;
}

static JSValue
js_element_removeAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));
	dom_exception exc;
	dom_string *attr_name = NULL;

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	exc = dom_string_create((const uint8_t *)str, len, &attr_name);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !attr_name) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	dom_element_remove_attribute(el, attr_name);
	dom_string_unref(attr_name);

	return JS_UNDEFINED;
}

static JSValue
js_element_removeChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el || !JS_IsObject(argv[0])) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	JSValue node = argv[0];
	dom_node *el2 = (dom_node *)(js_getopaque(node, js_element_class_id));
	dom_exception exc;
	dom_node *spare;
	exc = dom_node_remove_child(el, el2, &spare);

	if (exc == DOM_NO_ERR && spare) {
		interpreter->changed = 1;
		//dom_node_unref(el);

		return getElement(ctx, spare);
	}
	//dom_node_unref(el);

	return JS_NULL;
}

static JSValue
js_element_replaceWith(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

// TODO

#if 0
	if (argc < 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(js_getopaque(this_val, js_element_class_id));

	if (!el || !JS_IsObject(argv[0])) {
		return JS_UNDEFINED;
	}
	JSValue replacement = argv[0];
	xmlpp::Node *rep = static_cast<xmlpp::Node *>(js_getopaque(replacement, js_element_class_id));
	auto n = xmlAddPrevSibling(el->cobj(), rep->cobj());
	xmlpp::Node::create_wrapper(n);
	xmlpp::Node::remove_node(el);
	interpreter->changed = 1;
#endif

	return JS_UNDEFINED;
}

static JSValue
js_element_setAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 2) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *el = (dom_node *)(js_getopaque(this_val, js_element_class_id));

	if (!el) {
		return JS_UNDEFINED;
	}
	//dom_node_ref(el);

	const char *attr;
	const char *value;
	size_t attr_len, value_len;
	attr = JS_ToCStringLen(ctx, &attr_len, argv[0]);

	if (!attr) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	value = JS_ToCStringLen(ctx, &value_len, argv[1]);

	if (!value) {
		JS_FreeCString(ctx, attr);
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}

	dom_exception exc;
	dom_string *attr_str = NULL, *value_str = NULL;

	exc = dom_string_create((const uint8_t *)attr, attr_len, &attr_str);
	JS_FreeCString(ctx, attr);

	if (exc != DOM_NO_ERR || !attr_str) {
		JS_FreeCString(ctx, value);
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_string_create((const uint8_t *)value, value_len, &value_str);
	JS_FreeCString(ctx, value);

	if (exc != DOM_NO_ERR) {
		dom_string_unref(attr_str);
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}

	exc = dom_element_set_attribute(el,
			attr_str, value_str);
	dom_string_unref(attr_str);
	dom_string_unref(value_str);

	if (exc != DOM_NO_ERR) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	interpreter->changed = 1;
	//dom_node_unref(el);

	return JS_UNDEFINED;
}

static JSValue
js_element_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[element object]");
}

static const JSCFunctionListEntry js_element_proto_funcs[] = {
	JS_CGETSET_DEF("attributes",	js_element_get_property_attributes, NULL),
	JS_CGETSET_DEF("checked",	js_element_get_property_checked, js_element_set_property_checked),
	JS_CGETSET_DEF("children",	js_element_get_property_children, NULL),
	JS_CGETSET_DEF("childElementCount",	js_element_get_property_childElementCount, NULL),
	JS_CGETSET_DEF("childNodes",	js_element_get_property_childNodes, NULL),
	JS_CGETSET_DEF("classList",	js_element_get_property_classList, NULL),
	JS_CGETSET_DEF("className",	js_element_get_property_className, js_element_set_property_className),
//	JS_CGETSET_DEF("clientHeight",	js_element_get_property_clientHeight, NULL),
//	JS_CGETSET_DEF("clientLeft",	js_element_get_property_clientLeft, NULL),
//	JS_CGETSET_DEF("clientTop",	js_element_get_property_clientTop, NULL),
//	JS_CGETSET_DEF("clientWidth",	js_element_get_property_clientWidth, NULL),
	JS_CGETSET_DEF("dataset",	js_element_get_property_dataset, NULL),
	JS_CGETSET_DEF("dir",	js_element_get_property_dir, js_element_set_property_dir),
	JS_CGETSET_DEF("firstChild",	js_element_get_property_firstChild, NULL),
	JS_CGETSET_DEF("firstElementChild",	js_element_get_property_firstElementChild, NULL),
	JS_CGETSET_DEF("href",	js_element_get_property_href, js_element_set_property_href),
	JS_CGETSET_DEF("id",	js_element_get_property_id, js_element_set_property_id),
	JS_CGETSET_DEF("innerHTML",	js_element_get_property_innerHtml, js_element_set_property_innerHtml),
	JS_CGETSET_DEF("innerText",	js_element_get_property_innerHtml, js_element_set_property_innerText),
	JS_CGETSET_DEF("lang",	js_element_get_property_lang, js_element_set_property_lang),
	JS_CGETSET_DEF("lastChild",	js_element_get_property_lastChild, NULL),
	JS_CGETSET_DEF("lastElementChild",	js_element_get_property_lastElementChild, NULL),
	JS_CGETSET_DEF("nextElementSibling",	js_element_get_property_nextElementSibling, NULL),
	JS_CGETSET_DEF("nextSibling",	js_element_get_property_nextSibling, NULL),
	JS_CGETSET_DEF("nodeName",	js_element_get_property_nodeName, NULL),
	JS_CGETSET_DEF("nodeType",	js_element_get_property_nodeType, NULL),
	JS_CGETSET_DEF("nodeValue",	js_element_get_property_nodeValue, NULL),
//	JS_CGETSET_DEF("offsetHeight",	js_element_get_property_offsetHeight, NULL),
//	JS_CGETSET_DEF("offsetLeft",	js_element_get_property_offsetLeft, NULL),
	JS_CGETSET_DEF("offsetParent",	js_element_get_property_offsetParent, NULL),
//	JS_CGETSET_DEF("offsetTop",	js_element_get_property_offsetTop, NULL),
//	JS_CGETSET_DEF("offsetWidth",	js_element_get_property_offsetWidth, NULL),
	JS_CGETSET_DEF("outerHTML",	js_element_get_property_outerHtml, js_element_set_property_outerHtml),
	JS_CGETSET_DEF("ownerDocument",	js_element_get_property_ownerDocument, NULL),
	JS_CGETSET_DEF("parentElement",	js_element_get_property_parentElement, NULL),
	JS_CGETSET_DEF("parentNode",	js_element_get_property_parentNode, NULL),
	JS_CGETSET_DEF("previousElementSibling",	js_element_get_property_previousElementSibling, NULL),
	JS_CGETSET_DEF("previousSibling",	js_element_get_property_previousSibling, NULL),
	JS_CGETSET_DEF("style",		js_element_get_property_style, NULL),
	JS_CGETSET_DEF("tagName",	js_element_get_property_tagName, NULL),
	JS_CGETSET_DEF("textContent",	js_element_get_property_textContent, js_element_set_property_textContent),
	JS_CGETSET_DEF("title",	js_element_get_property_title, js_element_set_property_title),
	JS_CGETSET_DEF("value",	js_element_get_property_value, js_element_set_property_value),
	JS_CFUNC_DEF("addEventListener",	3, js_element_addEventListener),
	JS_CFUNC_DEF("appendChild",	1, js_element_appendChild),
	JS_CFUNC_DEF("blur",		0, js_element_blur),
	JS_CFUNC_DEF("click",		0, js_element_click),
	JS_CFUNC_DEF("cloneNode",	1, js_element_cloneNode),
	JS_CFUNC_DEF("closest",		1, js_element_closest),
	JS_CFUNC_DEF("contains",	1, js_element_contains),
	JS_CFUNC_DEF("dispatchEvent",	1, js_element_dispatchEvent),
	JS_CFUNC_DEF("focus",		0, js_element_focus),
	JS_CFUNC_DEF("getAttribute",	1,	js_element_getAttribute),
	JS_CFUNC_DEF("getAttributeNode",1,	js_element_getAttributeNode),
	JS_CFUNC_DEF("getBoundingClientRect",	0,	js_element_getBoundingClientRect),
	JS_CFUNC_DEF("getElementsByTagName", 1,	js_element_getElementsByTagName),
	JS_CFUNC_DEF("hasAttribute",	1,	js_element_hasAttribute),
	JS_CFUNC_DEF("hasAttributes",	0,	js_element_hasAttributes),
	JS_CFUNC_DEF("hasChildNodes",	0,	js_element_hasChildNodes),
	JS_CFUNC_DEF("insertBefore",	2,	js_element_insertBefore),
	JS_CFUNC_DEF("isEqualNode",	1, js_element_isEqualNode),
	JS_CFUNC_DEF("isSameNode",	1,		js_element_isSameNode),
	JS_CFUNC_DEF("matches",1,		js_element_matches),
	JS_CFUNC_DEF("querySelector",1,		js_element_querySelector),
	JS_CFUNC_DEF("querySelectorAll",1,		js_element_querySelectorAll),
	JS_CFUNC_DEF("remove",	0,	js_element_remove),
	JS_CFUNC_DEF("removeAttribute",	1,	js_element_removeAttribute),
	JS_CFUNC_DEF("removeChild",1,	js_element_removeChild),
	JS_CFUNC_DEF("removeEventListener",	3, js_element_removeEventListener),
	JS_CFUNC_DEF("replaceWith",1,	js_element_replaceWith),
	JS_CFUNC_DEF("setAttribute",2,	js_element_setAttribute),

	JS_CFUNC_DEF("toString", 0, js_element_toString)
};

void *map_elements;
//static std::map<void *, JSValueConst> map_elements;

static
void js_element_finalizer(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct js_element_private *el_private = (struct js_element_private *)JS_GetOpaque(val, js_element_class_id);

	if (el_private) {
		struct element_listener *l;
		dom_node *el = (dom_node *)el_private->node;

		if (el_private->listener) {
			dom_event_listener_unref(el_private->listener);
		}

		if (el) {
			void *old_node_data = NULL;
			dom_node_set_user_data(el, corestring_dom___ns_key_html_content_data, NULL, js_html_document_user_data_handler,
				(void *) &old_node_data);

			if (el->refcnt > 0) {
				dom_node_unref(el);
			}
		}

		foreach(l, el_private->listeners) {
			mem_free_set(&l->typ, NULL);
			JS_FreeValueRT(rt, l->fun);
		}
		free_list(el_private->listeners);
		JS_FreeValueRT(rt, el_private->thisval);

		attr_erase_from_map_str(map_elements, el_private->node);
		mem_free(el_private);
	}
}

static void
js_element_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct js_element_private *el_private = (struct js_element_private *)JS_GetOpaque(val, js_element_class_id);

	if (el_private) {
		JS_MarkValue(rt, el_private->thisval, mark_func);

		struct element_listener *l;

		foreach(l, el_private->listeners) {
			JS_MarkValue(rt, l->fun, mark_func);
		}
	}
}


static JSClassDef js_element_class = {
	"Element",
	.finalizer = js_element_finalizer,
	.gc_mark = js_element_mark,
};

int
js_element_init(JSContext *ctx)
{
	JSValue element_proto;

	/* create the element class */
	JS_NewClassID(&js_element_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_element_class_id, &js_element_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	element_proto = JS_NewObject(ctx);
	REF_JS(element_proto);

	JS_SetPropertyFunctionList(ctx, element_proto, js_element_proto_funcs, countof(js_element_proto_funcs));
	JS_SetClassProto(ctx, js_element_class_id, element_proto);
	JS_SetPropertyStr(ctx, global_obj, "Element", JS_DupValue(ctx, element_proto));

	JS_FreeValue(ctx, global_obj);

	return 0;
}

void *map_privates;

JSValue
getElement(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

//	JSValue second;
//	static int initialized;
//	/* create the element class */
//	if (!initialized) {
//		JS_NewClassID(&js_element_class_id);
//		JS_NewClass(JS_GetRuntime(ctx), js_element_class_id, &js_element_class);
//		initialized = 1;
//	}
//	second = attr_find_in_map(map_elements, node);
//
//	if (!JS_IsNull(second)) {
//		JSValue r = JS_DupValue(ctx, second);
//		RETURN_JS(r);
//	}
	struct js_element_private *el_private = (struct js_element_private *)mem_calloc(1, sizeof(*el_private));

	if (!el_private) {
		return JS_NULL;
	}
	init_list(el_private->listeners);
	el_private->node = node;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	el_private->interpreter = interpreter;

	JSValue element_obj = JS_NewObjectClass(ctx, js_element_class_id);
	REF_JS(element_obj);

	JS_SetPropertyFunctionList(ctx, element_obj, js_element_proto_funcs, countof(js_element_proto_funcs));
	JS_SetClassProto(ctx, js_element_class_id, element_obj);
	dom_node_ref((dom_node *)node);
	JS_SetOpaque(element_obj, el_private);

	attr_save_in_map(map_elements, node, element_obj);
	attr_save_in_map_void(map_privates, node, el_private);

	void *old_node_data = NULL;
	dom_node_set_user_data(node,
				     corestring_dom___ns_key_html_content_data,
				     (void *)node, js_html_document_user_data_handler,
				     (void *) &old_node_data);

	JSValue rr = JS_DupValue(ctx, element_obj);
	el_private->thisval = JS_DupValue(ctx, rr);
	RETURN_JS(rr);
}

void
check_element_event(void *interp, void *elem, const char *event_name, struct term_event *ev)
{
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)interp;
	JSContext *ctx = (JSContext *)interpreter->backend_data;
	struct js_element_private *el_private = attr_find_in_map_void(map_privates, elem);

	if (!el_private) {
		return;
	}
	interpreter->heartbeat = add_heartbeat(interpreter);

	struct element_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, event_name)) {
			continue;
		}
		if (ev && ev->ev == EVENT_KBD && (!strcmp(event_name, "keydown") || !strcmp(event_name, "keyup") || !strcmp(event_name, "keypress"))) {
			JSValue func = JS_DupValue(ctx, l->fun);
			JSValue arg = get_keyboardEvent(ctx, ev);
			JSValue ret = JS_Call(ctx, func, el_private->thisval, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		} else {
			JSValue func = JS_DupValue(ctx, l->fun);
			JSValue arg = JS_UNDEFINED;
			JSValue ret = JS_Call(ctx, func, el_private->thisval, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
	}
	done_heartbeat(interpreter->heartbeat);
	check_for_rerender(interpreter, event_name);
}

static void
element_event_handler(dom_event *event, void *pw)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_element_private *el_private = (struct js_element_private *)pw;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)el_private->interpreter;
	JSContext *ctx = (JSContext *)interpreter->backend_data;
	//dom_node *el = (dom_node *)el_private->node;

	if (!event) {
		return;
	}
	//dom_node_ref(el);

	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		//dom_node_unref(el);
		return;
	}
//	interpreter->heartbeat = add_heartbeat(interpreter);

	struct element_listener *l, *next;

	foreachsafe(l, next, el_private->listeners) {
		if (strcmp(l->typ, dom_string_data(typ))) {
			continue;
		}
		JSValue func = JS_DupValue(ctx, l->fun);
		JSValue arg = getEvent(ctx, event);
		JSValue ret = JS_Call(ctx, func, el_private->thisval, 1, (JSValueConst *) &arg);
		JS_FreeValue(ctx, ret);
		JS_FreeValue(ctx, func);
		JS_FreeValue(ctx, arg);
	}
//	done_heartbeat(interpreter->heartbeat);
	check_for_rerender(interpreter, dom_string_data(typ));
	dom_string_unref(typ);
	//dom_node_unref(el);
}
