/* The MuJS html Text objects implementation. */

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

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/libdom/corestrings.h"
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#include "document/libdom/renderer2.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/mujs/mapa.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/attr.h"
#include "ecmascript/mujs/attributes.h"
#include "ecmascript/mujs/collection.h"
#include "ecmascript/mujs/dataset.h"
#include "ecmascript/mujs/document.h"
#include "ecmascript/mujs/domrect.h"
#include "ecmascript/mujs/element.h"
#include "ecmascript/mujs/event.h"
#include "ecmascript/mujs/fragment.h"
#include "ecmascript/mujs/keyboard.h"
#include "ecmascript/mujs/nodelist.h"
#include "ecmascript/mujs/nodelist2.h"
#include "ecmascript/mujs/style.h"
#include "ecmascript/mujs/text.h"
#include "ecmascript/mujs/tokenlist.h"
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

struct text_listener {
	LIST_HEAD_EL(struct text_listener);
	char *typ;
	const char *fun;
};

struct mjs_text_private {
	struct ecmascript_interpreter *interpreter;
	const char *thisval;
	LIST_OF(struct text_listener) listeners;
	dom_event_listener *listener;
	void *node;
	int ref_count;
};

static void text_event_handler(dom_event *event, void *pw);
static void mjs_text_dispatchEvent(js_State *J);
static void mjs_text_set_property_textContent(js_State *J);

void *
mjs_getprivate_text(js_State *J, int idx)
{
	struct mjs_text_private *priv = (struct mjs_text_private *)js_touserdata(J, idx, "text");

	if (!priv) {
		return NULL;
	}

	return priv->node;
}

static void
mjs_text_get_property_children(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	mjs_push_nodelist(J, nodes);
	dom_nodelist_unref(nodes);
}

static void
mjs_text_get_property_childElementCount(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t res = 0;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	exc = dom_nodelist_get_length(nodes, &res);
	dom_nodelist_unref(nodes);
	js_pushnumber(J, res);
}

static void
mjs_text_get_property_childNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	mjs_push_nodelist(J, nodes);
	dom_nodelist_unref(nodes);
}

static void
mjs_text_get_property_firstChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_first_child(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

static void
mjs_text_get_property_firstElementChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t size = 0;
	uint32_t i;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	exc = dom_nodelist_get_length(nodes, &size);

	if (exc != DOM_NO_ERR || !size) {
		dom_nodelist_unref(nodes);
		js_pushnull(J);
		return;
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
			mjs_push_element(J, child);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(child);
			return;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	js_pushnull(J);
}

static void
mjs_text_get_property_lastChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *last_child = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_last_child(el, &last_child);

	if (exc != DOM_NO_ERR || !last_child) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, last_child);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(last_child);
}

static void
mjs_text_get_property_lastElementChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t size = 0;
	int i;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	exc = dom_nodelist_get_length(nodes, &size);

	if (exc != DOM_NO_ERR || !size) {
		dom_nodelist_unref(nodes);
		js_pushnull(J);
		return;
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
			mjs_push_element(J, child);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(child);
			return;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	js_pushnull(J);
}

#if 0
static void
mjs_text_get_property_nextElementSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *node;
	dom_node *prev_next = NULL;

	if (!el) {
		js_pushnull(J);
		return;
	}
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
			js_pushnull(J);
			return;
		}
		exc = dom_node_get_node_type(next, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			mjs_push_element(J, next);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(next);
			return;
		}
		prev_next = next;
		node = next;
	}
	js_pushnull(J);
}
#endif

static void
mjs_text_get_property_nodeName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_string *name = NULL;
	dom_exception exc;

	if (!node) {
		js_pushstring(J, "");
		return;
	}
	exc = dom_node_get_node_name(node, &name);

	if (exc != DOM_NO_ERR || !name) {
		js_pushstring(J, "");
		return;
	}
	js_pushstring(J, dom_string_data(name));
	dom_string_unref(name);
}

static void
mjs_text_get_property_nodeType(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node_type type;
	dom_exception exc;

	if (!node) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_node_type(node, &type);

	if (exc == DOM_NO_ERR) {
		js_pushnumber(J, type);
		return;
	}
	js_pushnull(J);
}

static void
mjs_text_get_property_nodeValue(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_string *content = NULL;
	dom_exception exc;

	if (!node) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_node_value(node, &content);

	if (exc != DOM_NO_ERR || !content) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(content));
	dom_string_unref(content);
}

static void
mjs_text_set_property_nodeValue(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!node) {
		js_error(J, "error");
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	dom_string *value = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, strlen(str), &value);

	if (exc != DOM_NO_ERR || !value) {
		js_pushundefined(J);
		return;
	}
	exc = dom_node_set_node_value(node, value);
	dom_string_unref(value);

	js_pushundefined(J);
}

static void
mjs_text_get_property_nextSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_next_sibling(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

static void
mjs_text_get_property_ownerDocument(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	mjs_push_document(J, interpreter);
}

static void
mjs_text_get_property_parentElement(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

static void
mjs_text_get_property_parentNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

#if 0
static void
mjs_text_get_property_previousElementSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *node;
	dom_node *prev_prev = NULL;

	if (!el) {
		js_pushnull(J);
		return;
	}
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
			js_pushnull(J);
			return;
		}
		exc = dom_node_get_node_type(prev, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			mjs_push_element(J, prev);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(prev);
			return;
		}
		prev_prev = prev;
		node = prev;
	}
	js_pushnull(J);
}
#endif

static void
mjs_text_get_property_previousSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_previous_sibling(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

static void
mjs_text_get_property_textContent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_string *content = NULL;
	dom_exception exc = dom_node_get_text_content(el, &content);

	if (exc != DOM_NO_ERR || !content) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(content));
	dom_string_unref(content);
}

static void
mjs_text_set_property_textContent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	dom_string *content = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, strlen(str), &content);

	if (exc != DOM_NO_ERR || !content) {
		js_error(J, "error");
		return;
	}
	exc = dom_node_set_text_content(el, content);
	dom_string_unref(content);

	js_pushundefined(J);
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

static void
mjs_text_addEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_text_private *el_private = (struct mjs_text_private *)js_touserdata(J, 0, "text");

	if (!el_private) {
		js_pushnull(J);
		return;
	}
	dom_node *el = el_private->node;

	if (!el) {
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
	struct text_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}

		if (l->fun == fun) {
			mem_free(method);
			js_pushundefined(J);
			return;
		}
	}
	struct text_listener *n = (struct text_listener *)mem_calloc(1, sizeof(*n));

	if (!n) {
		js_pushundefined(J);
		return;
	}
	n->fun = fun;
	n->typ = method;
	add_to_list_end(el_private->listeners, n);
	dom_exception exc;

	if (el_private->listener) {
		dom_event_listener_ref(el_private->listener);
	} else {
		exc = dom_event_listener_create(text_event_handler, el_private, &el_private->listener);

		if (exc != DOM_NO_ERR || !el_private->listener) {
			js_pushundefined(J);
			return;
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
	js_pushundefined(J);
}

static void
mjs_text_removeEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_text_private *el_private = (struct mjs_text_private *)js_touserdata(J, 0, "text");

	if (!el_private) {
		js_pushnull(J);
		return;
	}
	dom_node *el = el_private->node;

	if (!el) {
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
	struct text_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (l->fun == fun) {
			dom_string *typ = NULL;
			dom_exception exc = dom_string_create((const uint8_t *)method, strlen(method), &typ);

			if (exc != DOM_NO_ERR || !typ) {
				continue;
			}
			//dom_event_target_remove_event_listener(el, typ, el_private->listener, false);
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
mjs_text_appendChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *res = NULL;

	if (!el) {
		js_error(J, "error");
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate_any(J, 1));

	if (!el2) {
		js_error(J, "error");
		return;
	}
	dom_exception exc = dom_node_append_child(el, el2, &res);

	if (exc == DOM_NO_ERR && res) {
		interpreter->changed = 1;
		mjs_push_element(J, res);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(res);
		return;
	}
	js_error(J, "error");
}

static void
mjs_text_cloneNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_exception exc;
	bool deep = js_toboolean(J, 1);
	dom_node *clone = NULL;
	exc = dom_node_clone_node(el, deep, &clone);

	if (exc != DOM_NO_ERR || !clone) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, clone);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(clone);
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


static void
mjs_text_contains(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_error(J, "error");
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate_text(J, 1));

	if (!el2) {
		js_error(J, "error");
		return;
	}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_ref(el);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_ref(el2);

	while (1) {
		if (el == el2) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el2);
			js_pushboolean(J, 1);
			return;
		}
		dom_node *node = NULL;
		dom_exception exc = dom_node_get_parent_node(el2, &node);

		if (exc != DOM_NO_ERR || !node) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el2);
			js_pushboolean(J, 0);
			return;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(el2);
		el2 = node;
	}
}


static void
mjs_text_hasChildNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_exception exc;
	bool res;

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	exc = dom_node_has_child_nodes(el, &res);

	if (exc != DOM_NO_ERR) {
		js_pushboolean(J, 0);
		return;
	}
	js_pushboolean(J, res);
}

static void
mjs_text_insertBefore(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_error(J, "error");
		return;
	}
	dom_node *next_sibling = (dom_node *)(mjs_getprivate_any(J, 2));
	dom_node *child = (dom_node *)(mjs_getprivate_any(J, 1));

	if (!child) {
		js_error(J, "error");
		return;
	}
	dom_node *spare = NULL;
	dom_exception err = dom_node_insert_before(el, child, next_sibling, &spare);

	if (err != DOM_NO_ERR || !spare) {
		js_error(J, "error");
		return;
	}
	interpreter->changed = 1;
	mjs_push_element(J, spare);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(spare);
}

static void
mjs_text_isEqualNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate_text(J, 1));

	struct string first;
	struct string second;

	if (!init_string(&first)) {
		js_error(J, "out of memory");
		return;
	}
	if (!init_string(&second)) {
		done_string(&first);
		js_error(J, "out of memory");
		return;
	}
	ecmascript_walk_tree(&first, el, false, true);
	ecmascript_walk_tree(&second, el2, false, true);
	bool ret = !strcmp(first.source, second.source);
	done_string(&first);
	done_string(&second);
	js_pushboolean(J, ret);
}

static void
mjs_text_isSameNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate_text(J, 1));

	js_pushboolean(J, (el == el2));
}

static void
mjs_text_querySelector(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	const char *selector = js_tostring(J, 1);

	if (!selector) {
		js_pushnull(J);
		return;
	}
	void *ret = walk_tree_query(el, selector, 0);

	if (!ret) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, ret);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(ret);
}

static void
mjs_text_querySelectorAll(js_State *J)
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
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	const char *selector = js_tostring(J, 1);

	if (!selector) {
		js_pushnull(J);
		return;
	}
	LIST_OF(struct selector_node) *result_list = (LIST_OF(struct selector_node) *)mem_calloc(1, sizeof(*result_list));

	if (!result_list) {
		js_pushnull(J);
		return;
	}
	init_list(*result_list);
	walk_tree_query_append(el, selector, 0, result_list);
	mjs_push_nodelist2(J, result_list);
}

static void
mjs_text_removeChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));
	dom_node *el2 = (dom_node *)(mjs_getprivate_text(J, 1));
	dom_exception exc;
	dom_node *spare;
	exc = dom_node_remove_child(el, el2, &spare);

	if (exc == DOM_NO_ERR && spare) {
		interpreter->changed = 1;
		mjs_push_element(J, spare);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(spare);
		return;
	}
	js_pushnull(J);
}

static void
mjs_text_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[Text object]");
}

static void
mjs_text_finalizer(js_State *J, void *priv)
{
	struct mjs_text_private *el_private = (struct mjs_text_private *)priv;

	if (el_private) {
		dom_node *node = (dom_node *)(el_private->node);

		if (node && (node->refcnt > 0)) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(node);
		}
		mem_free(el_private);
	}
}

void
mjs_push_text(js_State *J, void *node)
{
	struct mjs_text_private *el_private = NULL;

#if 0
	void *second = attr_find_in_map(map_privates, node);

	if (second) {
		el_private = (struct mjs_text_private *)second;

		if (!attr_find_in_map(map_elements, el_private)) {
			el_private = NULL;
		} else {
			el_private->ref_count++;
		}
	}
#endif

	if (!el_private) {
		el_private = (struct mjs_text_private *)mem_calloc(1, sizeof(*el_private));

		if (!el_private) {
			js_pushnull(J);
			return;
		}
		el_private->node = node;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_ref((dom_node *)node);
	}

	js_newobject(J);
	{
		js_copy(J, 0);
		el_private->thisval = js_ref(J);
		js_newuserdata(J, "text", el_private, mjs_text_finalizer);
		addmethod(J, "addEventListener", mjs_text_addEventListener, 3);
		addmethod(J, "appendChild",mjs_text_appendChild, 1);
		addmethod(J, "cloneNode",	mjs_text_cloneNode, 1);
		addmethod(J, "contains",	mjs_text_contains, 1);
		addmethod(J, "dispatchEvent",	mjs_text_dispatchEvent, 1);
		addmethod(J, "hasChildNodes",	mjs_text_hasChildNodes, 0);
		addmethod(J, "insertBefore",	mjs_text_insertBefore, 2);
		addmethod(J, "isEqualNode",	mjs_text_isEqualNode, 1);
		addmethod(J, "isSameNode",		mjs_text_isSameNode, 1);
		addmethod(J, "querySelector",	mjs_text_querySelector, 1);
		addmethod(J, "querySelectorAll",	mjs_text_querySelectorAll, 1);
		addmethod(J, "removeChild",	mjs_text_removeChild, 1);
		addmethod(J, "removeEventListener", mjs_text_removeEventListener, 3);
		addmethod(J, "toString",		mjs_text_toString, 0);

		addproperty(J, "children",	mjs_text_get_property_children, NULL);
		addproperty(J, "childElementCount",	mjs_text_get_property_childElementCount, NULL);
		addproperty(J, "childNodes",	mjs_text_get_property_childNodes, NULL);
		addproperty(J, "firstChild",	mjs_text_get_property_firstChild, NULL);
		addproperty(J, "firstElementChild",	mjs_text_get_property_firstElementChild, NULL);
		addproperty(J, "lastChild",	mjs_text_get_property_lastChild, NULL);
		addproperty(J, "lastElementChild",	mjs_text_get_property_lastElementChild, NULL);
////		addproperty(J, "nextElementSibling",	mjs_text_get_property_nextElementSibling, NULL);
		addproperty(J, "nextSibling",	mjs_text_get_property_nextSibling, NULL);
		addproperty(J, "nodeName",	mjs_text_get_property_nodeName, NULL);
		addproperty(J, "nodeType",	mjs_text_get_property_nodeType, NULL);
		addproperty(J, "nodeValue",	mjs_text_get_property_nodeValue, mjs_text_set_property_nodeValue);
		addproperty(J, "ownerDocument",	mjs_text_get_property_ownerDocument, NULL);
		addproperty(J, "parentElement",	mjs_text_get_property_parentElement, NULL);
		addproperty(J, "parentNode",	mjs_text_get_property_parentNode, NULL);
////		addproperty(J, "previousElementSibling",	mjs_text_get_property_previousElementSibling, NULL);
		addproperty(J, "previousSibling",	mjs_text_get_property_previousSibling, NULL);
		addproperty(J, "textContent",	mjs_text_get_property_textContent, mjs_text_set_property_textContent);
	}
}

int
mjs_text_init(js_State *J)
{
#if 0
	mjs_push_element(J, NULL);
	js_defglobal(J, "text", JS_DONTENUM);
#endif
	return 0;
}

#if 0
void
check_element_event(void *interp, void *elem, const char *event_name, struct term_event *ev)
{
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)interp;
	js_State *J = (js_State *)interpreter->backend_data;
	void *second = attr_find_in_map(map_privates, elem);

	if (!second) {
		return;
	}
	struct mjs_text_private *el_private = (struct mjs_text_private *)second;

	struct text_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, event_name)) {
			continue;
		}

		if (ev && ev->ev == EVENT_KBD && (!strcmp(event_name, "keydown") || !strcmp(event_name, "keyup") || !strcmp(event_name, "keypress"))) {
			js_getregistry(J, l->fun); /* retrieve the js function from the registry */
			js_getregistry(J, el_private->thisval);
			mjs_push_keyboardEvent(J, ev, event_name);
			js_pcall(J, 1);
			js_pop(J, 1);
		} else {
			js_getregistry(J, l->fun); /* retrieve the js function from the registry */
			js_getregistry(J, el_private->thisval);
			js_pushundefined(J);
			js_pcall(J, 1);
			js_pop(J, 1);
		}
	}
	check_for_rerender(interpreter, event_name);
}
#endif

static void
mjs_text_dispatchEvent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_text(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_event *event = (dom_event *)js_touserdata(J, 1, "event");

	if (!event) {
		js_pushboolean(J, 0);
		return;
	}
	bool result = false;
	(void)dom_event_target_dispatch_event(el, event, &result);
	js_pushboolean(J, result);
}

static void
text_event_handler(dom_event *event, void *pw)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_text_private *el_private = (struct mjs_text_private *)pw;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)el_private->interpreter;
	js_State *J = (js_State *)interpreter->backend_data;

	if (!event) {
		return;
	}

	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		return;
	}
//	interpreter->heartbeat = add_heartbeat(interpreter);

	struct text_listener *l, *next;

	foreachsafe(l, next, el_private->listeners) {
		if (strcmp(l->typ, dom_string_data(typ))) {
			continue;
		}
		js_getregistry(J, l->fun); /* retrieve the js function from the registry */
		js_getregistry(J, el_private->thisval);
		mjs_push_event(J, event);
		js_pcall(J, 1);
		js_pop(J, 1);
	}
//	done_heartbeat(interpreter->heartbeat);
	check_for_rerender(interpreter, dom_string_data(typ));
	dom_string_unref(typ);
}
