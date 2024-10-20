/* The QuickJS html DocumentFragment objects implementation. */

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
#include "js/ecmascript.h"
#include "js/ecmascript-c.h"
#include "js/quickjs/mapa.h"
#include "js/quickjs.h"
#include "js/quickjs/attr.h"
#include "js/quickjs/attributes.h"
#include "js/quickjs/collection.h"
#include "js/quickjs/dataset.h"
#include "js/quickjs/domrect.h"
#include "js/quickjs/element.h"
#include "js/quickjs/event.h"
#include "js/quickjs/fragment.h"
#include "js/quickjs/heartbeat.h"
#include "js/quickjs/keyboard.h"
#include "js/quickjs/nodelist.h"
#include "js/quickjs/nodelist2.h"
#include "js/quickjs/style.h"
#include "js/quickjs/tokenlist.h"
#include "js/quickjs/window.h"
#include "session/session.h"
#include "terminal/event.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

JSClassID js_fragment_class_id;

struct fragment_listener {
	LIST_HEAD_EL(struct fragment_listener);
	char *typ;
	JSValue fun;
};

struct js_fragment_private {
	LIST_OF(struct fragment_listener) listeners;
	struct ecmascript_interpreter *interpreter;
	JSValue thisval;
	dom_event_listener *listener;
	void *node;
};

static void fragment_event_handler(dom_event *event, void *pw);
static JSValue js_fragment_set_property_textContent(JSContext *ctx, JSValueConst this_val, JSValue val);

void *
js_getopaque_fragment(JSValueConst obj, JSClassID class_id)
{
	REF_JS(obj);
	struct js_fragment_private *res = (struct js_fragment_private *)JS_GetOpaque(obj, class_id);

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

void *
fragment_get_node(JSValueConst obj)
{
	return js_getopaque_fragment(obj, js_fragment_class_id);
}

static JSValue
js_fragment_get_property_children(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
js_fragment_get_property_childElementCount(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
js_fragment_get_property_childNodes(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
js_fragment_get_property_firstChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_node *node = NULL;
	dom_exception exc;

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);

	return rr;
}

static JSValue
js_fragment_get_property_firstElementChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(child);
			return rr;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	//dom_node_unref(el);

	return JS_NULL;
}


static JSValue
js_fragment_get_property_lastChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(last_child);

	return rr;
}

static JSValue
js_fragment_get_property_lastElementChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(child);
			return rr;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	//dom_node_unref(el);

	return JS_NULL;
}

#if 0
static JSValue
js_fragment_get_property_nextElementSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(prev_next);
		}

		if (exc != DOM_NO_ERR || !next) {
			return JS_NULL;
		}
		exc = dom_node_get_node_type(next, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			JSValue rr = getElement(ctx, next);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(next);
			return rr;
		}
		prev_next = next;
		node = next;
	}

	return JS_NULL;
}
#endif

static JSValue
js_fragment_get_property_nodeName(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *node = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
js_fragment_get_property_nodeType(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *node = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
js_fragment_get_property_nodeValue(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *node = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
js_fragment_set_property_nodeValue(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *node = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

	if (!node) {
		return JS_UNDEFINED;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	dom_string *value = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, len, &value);
	JS_FreeCString(ctx, str);

	if (exc != DOM_NO_ERR || !value) {
		//dom_node_unref(el);
		return JS_UNDEFINED;
	}
	exc = dom_node_set_node_value(node, value);
	dom_string_unref(value);
	//dom_node_unref(el);
	interpreter->changed = true;

	return JS_UNDEFINED;
}

static JSValue
js_fragment_get_property_nextSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);

	return rr;
}



static JSValue
js_fragment_get_property_ownerDocument(JSContext *ctx, JSValueConst this_val)
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
js_fragment_get_property_parentElement(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)js_getopaque_fragment(this_val, js_fragment_class_id);
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);

	return rr;
}

static JSValue
js_fragment_get_property_parentNode(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_node *el = (dom_node *)js_getopaque_fragment(this_val, js_fragment_class_id);
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);

	return rr;
}

#if 0
static JSValue
js_fragment_get_property_previousElementSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(prev_prev);
		}

		if (exc != DOM_NO_ERR || !prev) {
			return JS_NULL;
		}
		exc = dom_node_get_node_type(prev, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			JSValue rr = getElement(ctx, prev);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(prev);
			return rr;
		}
		prev_prev = prev;
		node = prev;
	}
	//dom_node_unref(el);

	return JS_NULL;
}
#endif

static JSValue
js_fragment_get_property_previousSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);

	return rr;
}


static JSValue
js_fragment_get_property_textContent(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

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
js_fragment_set_property_textContent(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

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
js_fragment_dispatchEvent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct js_fragment_private *el_private = (struct js_fragment_private *)(js_getopaque_fragment(this_val, js_fragment_class_id));

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
	dom_event *event = (dom_event *)(js_getopaque_fragment(eve, js_event_class_id));

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
js_fragment_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct js_fragment_private *el_private = (struct js_fragment_private *)(js_getopaque_fragment(this_val, js_fragment_class_id));

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
	struct fragment_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}

		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			mem_free(method);
			return JS_UNDEFINED;
		}
	}
	struct fragment_listener *n = (struct fragment_listener *)mem_calloc(1, sizeof(*n));

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
		exc = dom_event_listener_create(fragment_event_handler, el_private, &el_private->listener);

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
js_fragment_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct js_fragment_private *el_private = (struct js_fragment_private *)(js_getopaque_fragment(this_val, js_fragment_class_id));

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

	struct fragment_listener *l;

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
js_fragment_appendChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
	dom_node *res = NULL;
	dom_exception exc;

	if (argc != 1) {
		return JS_EXCEPTION;
	}

	if (!el) {
		return JS_EXCEPTION;
	}
	//dom_node_ref(el);
	dom_node *el2 = NULL;

	if (!JS_IsNull(argv[0])) {
		el2 = (dom_node *)(js_getopaque_any(argv[0]));
	}

	if (!el2) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	exc = dom_node_append_child(el, el2, &res);

	if (exc == DOM_NO_ERR && res) {
		interpreter->changed = 1;
		//dom_node_unref(el);
		JSValue rr = getElement(ctx, res);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(res);

		return rr;
	}
	//dom_node_unref(el);

	return JS_EXCEPTION;
}

static JSValue
js_fragment_cloneNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
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
js_fragment_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

	if (!el) {
		return JS_EXCEPTION;
	}
	dom_node *el2 = NULL;

	if (!JS_IsNull(argv[0])) {
	//dom_node_ref(el);
		el2 = (dom_node *)(js_getopaque_fragment(argv[0], js_fragment_class_id));
	}

	if (!el2) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_ref(el2);

	while (1) {
		if (el == el2) {
			//dom_node_unref(el);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el2);
			return JS_TRUE;
		}
		dom_node *node = NULL;
		dom_exception exc = dom_node_get_parent_node(el2, &node);

		if (exc != DOM_NO_ERR || !node) {
			//dom_node_unref(el);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el2);
			return JS_FALSE;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(el2);
		el2 = node;
	}
}

static JSValue
js_fragment_hasChildNodes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));
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
js_fragment_insertBefore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

	if (!el) {
		return JS_EXCEPTION;
	}
	//dom_node_ref(el);
	JSValue next_sibling1 = argv[1];
	JSValue child1 = argv[0];

	dom_node *next_sibling = NULL;
	dom_node *child = NULL;

	if (!JS_IsNull(next_sibling1)) {
		next_sibling = (dom_node *)(js_getopaque_any(next_sibling1));
	}

	if (!JS_IsNull(child1)) {
		child = (dom_node *)(js_getopaque_any(child1));
	}

	if (!child) {
		return JS_EXCEPTION;
	}
	dom_node *spare = NULL;
	dom_exception err = dom_node_insert_before(el, child, next_sibling, &spare);

	if (err != DOM_NO_ERR || !spare) {
		//dom_node_unref(el);
		return JS_EXCEPTION;
	}
	interpreter->changed = 1;
	//dom_node_unref(el);

	JSValue rr = getElement(ctx, spare);
	dom_node_unref(spare);

	return rr;
}

static JSValue
js_fragment_isEqualNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

	if (!el) {
		return JS_EXCEPTION;
	}
	//dom_node_ref(el);
	dom_node *el2 = NULL;

	if (!JS_IsNull(argv[0])) {
		el2 = (dom_node *)(js_getopaque_fragment(argv[0], js_fragment_class_id));
	}

	if (!el2) {
		return JS_EXCEPTION;
	}

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
js_fragment_isSameNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

	if (!el) {
		return JS_EXCEPTION;
	}
	//dom_node_ref(el);
	dom_node *el2 = NULL;

	if (!JS_IsNull(argv[0])) {
		el2 = (dom_node *)(js_getopaque_fragment(argv[0], js_fragment_class_id));
	}
	bool res = (el == el2);
	//dom_node_unref(el);

	return JS_NewBool(ctx, res);
}

static JSValue
js_fragment_querySelector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

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
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref((dom_node *)ret);

	return rr;
}

static JSValue
js_fragment_querySelectorAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

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
js_fragment_removeChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	dom_node *el = (dom_node *)(js_getopaque_fragment(this_val, js_fragment_class_id));

	if (!el || !JS_IsObject(argv[0])) {
		return JS_NULL;
	}
	//dom_node_ref(el);
	JSValue node = argv[0];
	dom_node *el2 = (dom_node *)(js_getopaque_fragment(node, js_fragment_class_id));
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
js_fragment_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[fragment object]");
}

static const JSCFunctionListEntry js_fragment_proto_funcs[] = {
	JS_CGETSET_DEF("children",	js_fragment_get_property_children, NULL),
	JS_CGETSET_DEF("childElementCount",	js_fragment_get_property_childElementCount, NULL),
	JS_CGETSET_DEF("childNodes",	js_fragment_get_property_childNodes, NULL),
	JS_CGETSET_DEF("firstChild",	js_fragment_get_property_firstChild, NULL),
	JS_CGETSET_DEF("firstElementChild",	js_fragment_get_property_firstElementChild, NULL),
	JS_CGETSET_DEF("lastChild",	js_fragment_get_property_lastChild, NULL),
	JS_CGETSET_DEF("lastElementChild",	js_fragment_get_property_lastElementChild, NULL),
	JS_CGETSET_DEF("nextSibling",	js_fragment_get_property_nextSibling, NULL),
	JS_CGETSET_DEF("nodeName",	js_fragment_get_property_nodeName, NULL), // Node
	JS_CGETSET_DEF("nodeType",	js_fragment_get_property_nodeType, NULL), // Node
	JS_CGETSET_DEF("nodeValue",	js_fragment_get_property_nodeValue, js_fragment_set_property_nodeValue), // Node
	JS_CGETSET_DEF("ownerDocument",	js_fragment_get_property_ownerDocument, NULL), // Node
	JS_CGETSET_DEF("parentElement",	js_fragment_get_property_parentElement, NULL), // Node
	JS_CGETSET_DEF("parentNode",	js_fragment_get_property_parentNode, NULL), // Node
////	JS_CGETSET_DEF("previousElementSibling",	js_fragment_get_property_previousElementSibling, NULL),
	JS_CGETSET_DEF("previousSibling",	js_fragment_get_property_previousSibling, NULL), // Node
	JS_CGETSET_DEF("textContent",	js_fragment_get_property_textContent, js_fragment_set_property_textContent), // Node

	JS_CFUNC_DEF("addEventListener",	3, js_fragment_addEventListener),
	JS_CFUNC_DEF("appendChild",	1, js_fragment_appendChild), // Node
	JS_CFUNC_DEF("cloneNode",	1, js_fragment_cloneNode), // Node
	JS_CFUNC_DEF("contains",	1, js_fragment_contains), // Node
	JS_CFUNC_DEF("dispatchEvent",	1, js_fragment_dispatchEvent),
	JS_CFUNC_DEF("hasChildNodes",	0,	js_fragment_hasChildNodes), // Node
	JS_CFUNC_DEF("insertBefore",	2,	js_fragment_insertBefore), // Node
	JS_CFUNC_DEF("isEqualNode",	1, js_fragment_isEqualNode), // Node
	JS_CFUNC_DEF("isSameNode",	1,		js_fragment_isSameNode), // Node
	JS_CFUNC_DEF("querySelector",1,		js_fragment_querySelector),
	JS_CFUNC_DEF("querySelectorAll",1,		js_fragment_querySelectorAll),
	JS_CFUNC_DEF("removeChild",1,	js_fragment_removeChild), // Node
	JS_CFUNC_DEF("removeEventListener",	3, js_fragment_removeEventListener),
	JS_CFUNC_DEF("toString", 0, js_fragment_toString)
};


static
void js_fragment_finalizer(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct js_fragment_private *el_private = (struct js_fragment_private *)JS_GetOpaque(val, js_fragment_class_id);

	if (el_private) {
		struct fragment_listener *l;
		dom_node *el = (dom_node *)el_private->node;

		if (el_private->listener) {
			dom_event_listener_unref(el_private->listener);
		}

		if (el) {
//			void *old_node_data = NULL;
//			dom_node_set_user_data(el, corestring_dom___ns_key_html_content_data, NULL, js_html_document_user_data_handler,
//				(void *) &old_node_data);

			if (el->refcnt > 0) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
				dom_node_unref(el);
			}
		}

		foreach(l, el_private->listeners) {
			mem_free_set(&l->typ, NULL);
			JS_FreeValueRT(rt, l->fun);
		}
		free_list(el_private->listeners);
		JS_FreeValueRT(rt, el_private->thisval);

		mem_free(el_private);
	}
}

static void
js_fragment_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct js_fragment_private *el_private = (struct js_fragment_private *)JS_GetOpaque(val, js_fragment_class_id);

	if (el_private) {
		JS_MarkValue(rt, el_private->thisval, mark_func);

		struct fragment_listener *l;

		foreach(l, el_private->listeners) {
			JS_MarkValue(rt, l->fun, mark_func);
		}
	}
}

static JSClassDef js_fragment_class = {
	"DocumentFragment",
	.finalizer = js_fragment_finalizer,
	.gc_mark = js_fragment_mark,
};

int
js_fragment_init(JSContext *ctx)
{
	JSValue fragment_proto;

	/* create the fragment class */
	JS_NewClassID(&js_fragment_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_fragment_class_id, &js_fragment_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	fragment_proto = JS_NewObject(ctx);
	REF_JS(fragment_proto);

	JS_SetPropertyFunctionList(ctx, fragment_proto, js_fragment_proto_funcs, countof(js_fragment_proto_funcs));
	JS_SetClassProto(ctx, js_fragment_class_id, fragment_proto);
	JS_SetPropertyStr(ctx, global_obj, "DocumentFragment", JS_DupValue(ctx, fragment_proto));

	JS_FreeValue(ctx, global_obj);

	return 0;
}

JSValue
getDocumentFragment(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_fragment_private *el_private = (struct js_fragment_private *)mem_calloc(1, sizeof(*el_private));

	if (!el_private) {
		return JS_NULL;
	}
	init_list(el_private->listeners);
	el_private->node = node;

	JSValue fragment_obj = JS_NewObjectClass(ctx, js_fragment_class_id);
	REF_JS(fragment_obj);

	JS_SetPropertyFunctionList(ctx, fragment_obj, js_fragment_proto_funcs, countof(js_fragment_proto_funcs));
	JS_SetClassProto(ctx, js_fragment_class_id, fragment_obj);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_ref((dom_node *)node);
	JS_SetOpaque(fragment_obj, el_private);

	JSValue rr = JS_DupValue(ctx, fragment_obj);
	RETURN_JS(rr);
}

static void
fragment_event_handler(dom_event *event, void *pw)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_fragment_private *el_private = (struct js_fragment_private *)pw;
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

	struct fragment_listener *l, *next;

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
