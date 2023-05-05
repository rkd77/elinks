/* The MuJS html element objects implementation. */

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
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/mujs/mapa.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/attr.h"
#include "ecmascript/mujs/attributes.h"
#include "ecmascript/mujs/collection.h"
#include "ecmascript/mujs/document.h"
#include "ecmascript/mujs/element.h"
#include "ecmascript/mujs/keyboard.h"
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

struct listener {
	LIST_HEAD(struct listener);
	char *typ;
	const char *fun;
};

struct mjs_element_private {
	struct ecmascript_interpreter *interpreter;
	const char *thisval;
	LIST_OF(struct listener) listeners;
	void *node;
};

static void *
mjs_getprivate(js_State *J, int idx)
{
	struct mjs_element_private *priv = (struct mjs_element_private *)js_touserdata(J, idx, "element");

	if (!priv) {
		return NULL;
	}

	return priv->node;
}

static void
mjs_element_get_property_attributes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_namednodemap *attrs = NULL;
	dom_exception exc = dom_node_get_attributes(el, &attrs);

	if (exc != DOM_NO_ERR || !attrs) {
		js_pushnull(J);
		return;
	}
	mjs_push_attributes(J, attrs);
}

static void
mjs_element_get_property_children(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
}

static void
mjs_element_get_property_childElementCount(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
mjs_element_get_property_childNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
}

static void
mjs_element_get_property_className(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_string *classstr = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_class, &classstr);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!classstr) {
		js_pushstring(J, "");
		return;
	} else {
		js_pushstring(J, dom_string_data(classstr));
		dom_string_unref(classstr);
	}
}

static void
mjs_element_get_property_dir(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_string *dir = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_dir, &dir);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!dir) {
		js_pushstring(J, "");
		return;
	} else {
		if (strcmp(dom_string_data(dir), "auto") && strcmp(dom_string_data(dir), "ltr") && strcmp(dom_string_data(dir), "rtl")) {
			js_pushstring(J, "");
		} else {
			js_pushstring(J, dom_string_data(dir));
		}
		dom_string_unref(dir);
	}
}

static void
mjs_element_get_property_firstChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
}

static void
mjs_element_get_property_firstElementChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
			return;
		}
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	js_pushnull(J);
}

static void
mjs_element_get_property_id(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_string *id = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_id, &id);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!id) {
		js_pushstring(J, "");
		return;
	} else {
		js_pushstring(J, dom_string_data(id));
		dom_string_unref(id);
	}
}

static void
mjs_element_get_property_lang(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_string *lang = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_lang, &lang);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!lang) {
		js_pushstring(J, "");
	} else {
		js_pushstring(J, dom_string_data(lang));
		dom_string_unref(lang);
	}
}

static void
mjs_element_get_property_lastChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
}

static void
mjs_element_get_property_lastElementChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
			return;
		}
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	js_pushnull(J);
}

static void
mjs_element_get_property_nextElementSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
			dom_node_unref(prev_next);
		}

		if (exc != DOM_NO_ERR || !next) {
			js_pushnull(J);
			return;
		}
		exc = dom_node_get_node_type(next, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			mjs_push_element(J, next);
			return;
		}
		prev_next = next;
		node = next;
	}
	js_pushnull(J);
}

static void
mjs_element_get_property_nodeName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate(J, 0));
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
mjs_element_get_property_nodeType(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate(J, 0));
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
mjs_element_get_property_nodeValue(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate(J, 0));
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
mjs_element_get_property_nextSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
}

static void
mjs_element_get_property_ownerDocument(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	mjs_push_document(J, interpreter);
}

static void
mjs_element_get_property_parentElement(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
}

static void
mjs_element_get_property_parentNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
}

static void
mjs_element_get_property_previousElementSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
			dom_node_unref(prev_prev);
		}

		if (exc != DOM_NO_ERR || !prev) {
			js_pushnull(J);
			return;
		}
		exc = dom_node_get_node_type(prev, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			mjs_push_element(J, prev);
			return;
		}
		prev_prev = prev;
		node = prev;
	}
	js_pushnull(J);
}

static void
mjs_element_get_property_previousSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
}

static void
mjs_element_get_property_tagName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_string *tag_name = NULL;
	dom_exception exc = dom_node_get_node_name(el, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(tag_name));
	dom_string_unref(tag_name);
}

static void
mjs_element_get_property_title(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_string *title = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_title, &title);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!title) {
		js_pushstring(J, "");
	} else {
		js_pushstring(J, dom_string_data(title));
		dom_string_unref(title);
	}
}

static bool
dump_node_element_attribute(struct string *buf, dom_node *node)
{
	dom_exception exc;
	dom_string *attr = NULL;
	dom_string *attr_value = NULL;

	exc = dom_attr_get_name((struct dom_attr *)node, &attr);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for dom_string_create\n");
		return false;
	}

	/* Get attribute's value */
	exc = dom_attr_get_value((struct dom_attr *)node, &attr_value);
	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for element_get_attribute\n");
		dom_string_unref(attr);
		return false;
	} else if (attr_value == NULL) {
		/* Element lacks required attribute */
		dom_string_unref(attr);
		return true;
	}

	add_char_to_string(buf, ' ');
	add_bytes_to_string(buf, dom_string_data(attr), dom_string_byte_length(attr));
	add_to_string(buf, "=\"");
	add_bytes_to_string(buf, dom_string_data(attr_value), dom_string_byte_length(attr_value));
	add_char_to_string(buf, '"');

	/* Finished with the attr dom_string */
	dom_string_unref(attr);
	dom_string_unref(attr_value);

	return true;
}

static bool
dump_element(struct string *buf, dom_node *node, bool toSortAttrs)
{
// TODO toSortAttrs
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type type;
	dom_namednodemap *attrs;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for node_get_node_type\n");
		return false;
	} else {
		if (type == DOM_TEXT_NODE) {
			dom_string *str;

			exc = dom_node_get_text_content(node, &str);

			if (exc == DOM_NO_ERR && str != NULL) {
				int length = dom_string_byte_length(str);
				const char *string_text = dom_string_data(str);

				if (!((length == 1) && (*string_text == '\n'))) {
					add_bytes_to_string(buf, string_text, length);
				}
				dom_string_unref(str);
			}
			return true;
		}
		if (type != DOM_ELEMENT_NODE) {
			/* Nothing to print */
			return true;
		}
	}

	/* Get element name */
	exc = dom_node_get_node_name(node, &node_name);
	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for get_node_name\n");
		return false;
	}

	add_char_to_string(buf, '<');
	//save_in_map(mapa, node, buf->length);

	/* Get string data and print element name */
	add_bytes_to_string(buf, dom_string_data(node_name), dom_string_byte_length(node_name));

	exc = dom_node_get_attributes(node, &attrs);

	if (exc == DOM_NO_ERR) {
		dom_ulong length;

		exc = dom_namednodemap_get_length(attrs, &length);

		if (exc == DOM_NO_ERR) {
			int i;

			for (i = 0; i < length; ++i) {
				dom_node *attr;

				exc = dom_namednodemap_item(attrs, i, &attr);

				if (exc == DOM_NO_ERR) {
					dump_node_element_attribute(buf, attr);
					dom_node_unref(attr);
				}
			}
		}
		dom_node_unref(attrs);
	}
	add_char_to_string(buf, '>');

	/* Finished with the node_name dom_string */
	dom_string_unref(node_name);

	return true;
}

void
walk_tree(struct string *buf, void *nod, bool start, bool toSortAttrs)
{
	dom_node *node = (dom_node *)(nod);
	dom_nodelist *children = NULL;
	dom_node_type type;
	dom_exception exc;
	uint32_t size = 0;

	if (!start) {
		exc = dom_node_get_node_type(node, &type);

		if (exc == DOM_NO_ERR && type == DOM_TEXT_NODE) {
			dom_string *content = NULL;
			exc = dom_node_get_text_content(node, &content);

			if (exc == DOM_NO_ERR && content) {
				add_bytes_to_string(buf, dom_string_data(content), dom_string_length(content));
				dom_string_unref(content);
			}
		} else if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dump_element(buf, node, toSortAttrs);
		}
	}
	exc = dom_node_get_child_nodes(node, &children);

	if (exc == DOM_NO_ERR && children) {
		exc = dom_nodelist_get_length(children, &size);
		uint32_t i;

		for (i = 0; i < size; i++) {
			dom_node *item = NULL;
			exc = dom_nodelist_item(children, i, &item);

			if (exc == DOM_NO_ERR && item) {
				walk_tree(buf, item, false, toSortAttrs);
				dom_node_unref(item);
			}
		}
		dom_nodelist_unref(children);
	}

	if (!start) {
		exc = dom_node_get_node_type(node, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dom_string *node_name = NULL;
			exc = dom_node_get_node_name(node, &node_name);

			if (exc == DOM_NO_ERR && node_name) {
				add_to_string(buf, "</");
				add_bytes_to_string(buf, dom_string_data(node_name), dom_string_length(node_name));
				add_char_to_string(buf, '>');
				dom_string_unref(node_name);
			}
		}
	}
}

static void
walk_tree_content(struct string *buf, dom_node *node)
{
	dom_node_type type;
	dom_nodelist *children = NULL;
	dom_exception exc;

	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR && type == DOM_TEXT_NODE) {
		dom_string *content = NULL;
		exc = dom_node_get_text_content(node, &content);

		if (exc == DOM_NO_ERR && content) {
			add_bytes_to_string(buf, dom_string_data(content), dom_string_length(content));
			dom_string_unref(content);
		}
	}
	exc = dom_node_get_child_nodes(node, &children);

	if (exc == DOM_NO_ERR && children) {
		uint32_t i, size;
		exc = dom_nodelist_get_length(children, &size);

		for (i = 0; i < size; i++) {
			dom_node *item = NULL;
			exc = dom_nodelist_item(children, i, &item);

			if (exc == DOM_NO_ERR && item) {
				walk_tree_content(buf, item);
				dom_node_unref(item);
			}
		}
		dom_nodelist_unref(children);
	}
}

static void
mjs_element_get_property_innerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct string buf;
	if (!init_string(&buf)) {
		js_error(J, "out of memory");
		return;
	}
	walk_tree(&buf, el, true, false);
	js_pushstring(J, buf.source);
	done_string(&buf);
}

static void
mjs_element_get_property_outerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct string buf;
	if (!init_string(&buf)) {
		js_error(J, "out of memory");
		return;
	}
	walk_tree(&buf, el, false, false);
	js_pushstring(J, buf.source);
	done_string(&buf);
}

static void
mjs_element_get_property_textContent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct string buf;
	if (!init_string(&buf)) {
		js_error(J, "out of memory");
		return;
	}
	walk_tree_content(&buf, el);
	js_pushstring(J, buf.source);
	done_string(&buf);
}

static void
mjs_element_set_property_className(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_string *classstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &classstr);

	if (exc == DOM_NO_ERR && classstr) {
		exc = dom_element_set_attribute(el, corestring_dom_class, classstr);
		interpreter->changed = true;
		dom_string_unref(classstr);
	}
	js_pushundefined(J);
}

static void
mjs_element_set_property_dir(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	if (!strcmp(str, "ltr") || !strcmp(str, "rtl") || !strcmp(str, "auto")) {
		dom_string *dir = NULL;
		exc = dom_string_create((const uint8_t *)str, strlen(str), &dir);

		if (exc == DOM_NO_ERR && dir) {
			exc = dom_element_set_attribute(el, corestring_dom_dir, dir);
			interpreter->changed = true;
			dom_string_unref(dir);
		}
	}
	js_pushundefined(J);
}

static void
mjs_element_set_property_id(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_string *idstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);
	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &idstr);

	if (exc == DOM_NO_ERR && idstr) {
		exc = dom_element_set_attribute(el, corestring_dom_id, idstr);
		interpreter->changed = true;
		dom_string_unref(idstr);
	}
	js_pushundefined(J);
}

static void
mjs_element_set_property_innerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *s = js_tostring(J, 1);

	if (!s) {
		js_error(J, "out of memory");
		return;
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

	error = dom_hubbub_parser_parse_chunk(parser, (const uint8_t*)s, strlen(s));
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
	interpreter->changed = true;
	js_pushundefined(J);
}

static void
mjs_element_set_property_innerText(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
//TODO 
#if 0
	const char *val = js_tostring(J, 1);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	auto children = el->get_children();
	auto it = children.begin();
	auto end = children.end();

	for (;it != end; ++it) {
		xmlpp::Node::remove_node(*it);
	}
	el->add_child_text(val);
	interpreter->changed = true;
#endif
	js_pushundefined(J);
}

static void
mjs_element_set_property_lang(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_string *langstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);
	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &langstr);

	if (exc == DOM_NO_ERR && langstr) {
		exc = dom_element_set_attribute(el, corestring_dom_lang, langstr);
		interpreter->changed = true;
		dom_string_unref(langstr);
	}
	js_pushundefined(J);
}

static void
mjs_element_set_property_title(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_string *titlestr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &titlestr);

	if (exc == DOM_NO_ERR && titlestr) {
		exc = dom_element_set_attribute(el, corestring_dom_title, titlestr);
		interpreter->changed = true;
		dom_string_unref(titlestr);
	}
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
check_contains(dom_node *node, dom_node *searched, bool *result_set, bool *result)
{
	dom_nodelist *children = NULL;
	dom_exception exc;
	uint32_t size = 0;
	uint32_t i;

	if (*result_set) {
		return;
	}
	exc = dom_node_get_child_nodes(node, &children);
	if (exc == DOM_NO_ERR && children) {
		exc = dom_nodelist_get_length(children, &size);

		for (i = 0; i < size; i++) {
			dom_node *item = NULL;
			exc = dom_nodelist_item(children, i, &item);

			if (exc == DOM_NO_ERR && item) {
				if (item == searched) {
					*result_set = true;
					*result = true;
					dom_node_unref(item);
					return;
				}
				check_contains(item, searched, result_set, result);
				dom_node_unref(item);
			}
		}
		dom_nodelist_unref(children);
	}
}

static void
mjs_element_addEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_element_private *el_private = (struct mjs_element_private *)js_touserdata(J, 0, "element");

	if (!el_private) {
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

	struct listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (!strcmp(l->fun, fun)) {
			mem_free(method);
			js_pushundefined(J);
			return;
		}
	}
	struct listener *n = (struct listener *)mem_calloc(1, sizeof(*n));

	if (n) {
		n->typ = method;
		n->fun = fun;
		add_to_list_end(el_private->listeners, n);
	}
	js_pushundefined(J);
}

static void
mjs_element_removeEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_element_private *el_private = (struct mjs_element_private *)js_touserdata(J, 0, "element");

	if (!el_private) {
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
	struct listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (!strcmp(l->fun, fun)) {
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			if (l->fun) js_unref(J, l->fun);
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
mjs_element_appendChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_node *res = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate(J, 1));
	exc = dom_node_append_child(el, el2, &res);

	if (exc == DOM_NO_ERR && res) {
		interpreter->changed = true;
		mjs_push_element(J, res);
		return;
	}
	js_pushnull(J);
}

static void
mjs_element_cloneNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

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
}

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

static void
mjs_element_closest(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
// TODO
#if 0
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);
	xmlpp::ustring css = str;
	xmlpp::ustring xpath = css2xpath(css);

	xmlpp::Node::NodeSet elements;

	try {
		elements = el->find(xpath);
	} catch (xmlpp::exception &e) {
		js_pushnull(J);
		return;
	}

	if (elements.size() == 0) {
		js_pushnull(J);
		return;
	}

	while (el)
	{
		for (auto node: elements)
		{
			if (isAncestor(el, node))
			{
				mjs_push_element(J, node);
				return;
			}
		}
		el = el->get_parent();
	}
#endif
	js_pushnull(J);
}

static void
mjs_element_contains(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate(J, 1));

	if (!el2) {
		js_pushboolean(J, 0);
		return;
	}

	bool result_set = false;
	bool result = false;

	check_contains(el, el2, &result_set, &result);
	js_pushboolean(J, result);
}

static void
mjs_element_getAttribute(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_exception exc;
	dom_string *attr_name = NULL;
	dom_string *attr_value = NULL;

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}

	exc = dom_string_create((const uint8_t *)str, strlen(str), &attr_name);

	if (exc != DOM_NO_ERR || !attr_name) {
		js_pushnull(J);
		return;
	}

	exc = dom_element_get_attribute(el, attr_name, &attr_value);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR || !attr_value) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(attr_value));
	dom_string_unref(attr_value);
}

static void
mjs_element_getAttributeNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_exception exc;
	dom_string *attr_name = NULL;
	dom_attr *attr = NULL;

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &attr_name);

	if (exc != DOM_NO_ERR || !attr_name) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute_node(el, attr_name, &attr);

	if (exc != DOM_NO_ERR || !attr) {
		js_pushnull(J);
		return;
	}
	mjs_push_attr(J, attr);
}

static void
mjs_element_getElementsByTagName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	dom_nodelist *nlist = NULL;
	dom_exception exc;
	dom_string *tagname = NULL;
	exc = dom_string_create((const uint8_t *)str, strlen(str), &tagname);

	if (exc != DOM_NO_ERR || !tagname) {
		js_pushnull(J);
		return;
	}

	exc = dom_element_get_elements_by_tag_name(el, tagname, &nlist);
	dom_string_unref(tagname);

	if (exc != DOM_NO_ERR || !nlist) {
		js_pushnull(J);
		return;
	}
	mjs_push_nodelist(J, nlist);
}

static void
mjs_element_hasAttribute(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	const char *s = js_tostring(J, 1);

	if (!s) {
		js_error(J, "out of memory");
		return;
	}
	dom_string *attr_name = NULL;
	dom_exception exc;
	bool res;
	exc = dom_string_create((const uint8_t *)s, strlen(s), &attr_name);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}

	exc = dom_element_has_attribute(el, attr_name, &res);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, res);
}

static void
mjs_element_hasAttributes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_exception exc;
	bool res;

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	exc = dom_node_has_attributes(el, &res);

	if (exc != DOM_NO_ERR) {
		js_pushboolean(J, 0);
		return;
	}
	js_pushboolean(J, res);
}

static void
mjs_element_hasChildNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
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
mjs_element_insertBefore(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	dom_node *next_sibling = (dom_node *)(mjs_getprivate(J, 2));

	if (!next_sibling) {
		js_pushnull(J);
		return;
	}

	dom_node *child = (dom_node *)(mjs_getprivate(J, 1));

	dom_exception err;
	dom_node *spare;

	err = dom_node_insert_before(el, child, next_sibling, &spare);
	if (err != DOM_NO_ERR) {
		js_pushundefined(J);
		return;
	}
	interpreter->changed = true;
	mjs_push_element(J, spare);
}

static void
mjs_element_isEqualNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate(J, 1));

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
	walk_tree(&first, el, false, true);
	walk_tree(&second, el2, false, true);
	bool ret = !strcmp(first.source, second.source);
	done_string(&first);
	done_string(&second);
	js_pushboolean(J, ret);
}

static void
mjs_element_isSameNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate(J, 1));

	js_pushboolean(J, (el == el2));
}

static void
mjs_element_matches(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
// TODO
#if 0
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	const char *str = js_tostring(J, 1);
	xmlpp::ustring css = str;
	xmlpp::ustring xpath = css2xpath(css);

	xmlpp::Node::NodeSet elements;

	try {
		elements = el->find(xpath);
	} catch (xmlpp::exception &e) {
		js_pushboolean(J, 0);
		return;
	}

	for (auto node: elements) {
		if (node == el) {
			js_pushboolean(J, 1);
			return;
		}
	}
#endif
	js_pushboolean(J, 0);
}

static void
mjs_element_querySelector(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
// TODO
#if 0
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	const char *str = js_tostring(J, 1);
	xmlpp::ustring css = str;
	xmlpp::ustring xpath = css2xpath(css);
	xmlpp::Node::NodeSet elements;

	try {
		elements = el->find(xpath);
	} catch (xmlpp::exception &e) {
		js_pushnull(J);
		return;
	}

	for (auto node: elements)
	{
		if (isAncestor(el, node))
		{
			mjs_push_element(J, node);
			return;
		}
	}
#endif
	js_pushnull(J);
}

static void
mjs_element_querySelectorAll(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
// TODO
#if 0
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	const char *str = js_tostring(J, 1);
	xmlpp::ustring css = str;
	xmlpp::ustring xpath = css2xpath(css);
	xmlpp::Node::NodeSet elements;
	xmlpp::Node::NodeSet *res = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!res) {
		js_pushnull(J);
		return;
	}

	try {
		elements = el->find(xpath);
	} catch (xmlpp::exception &e) {}

	for (auto node: elements)
	{
		if (isAncestor(el, node)) {
			res->push_back(node);
		}
	}
	mjs_push_collection(J, res);
#endif
	js_pushnull(J);
}

static void
mjs_element_remove(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
// TODO
#if 0

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	xmlpp::Node::remove_node(el);
	interpreter->changed = true;
#endif
	js_pushundefined(J);
}

static void
mjs_element_removeChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));
	dom_node *el2 = (dom_node *)(mjs_getprivate(J, 1));
	dom_exception exc;
	dom_node *spare;
	exc = dom_node_remove_child(el, el2, &spare);

	if (exc == DOM_NO_ERR && spare) {
		interpreter->changed = true;
		mjs_push_element(J, spare);
		return;
	}
	js_pushnull(J);
}

static void
mjs_element_replaceWith(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
// TODO
#if 0

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));
	xmlpp::Node *rep = static_cast<xmlpp::Node *>(mjs_getprivate(J, 1));

	if (!el || !rep) {
		js_pushundefined(J);
		return;
	}
	auto n = xmlAddPrevSibling(el->cobj(), rep->cobj());
	xmlpp::Node::create_wrapper(n);
	xmlpp::Node::remove_node(el);
	interpreter->changed = true;
#endif
	js_pushundefined(J);
}

static void
mjs_element_setAttribute(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *attr;
	const char *value;
	attr = js_tostring(J, 1);

	if (!attr) {
		js_error(J, "out of memory");
		return;
	}
	value = js_tostring(J, 2);

	if (!value) {
		js_error(J, "out of memory");
		return;
	}

	dom_exception exc;
	dom_string *attr_str = NULL, *value_str = NULL;

	exc = dom_string_create((const uint8_t *)attr, strlen(attr), &attr_str);

	if (exc != DOM_NO_ERR || !attr_str) {
		js_error(J, "error");
		return;
	}
	exc = dom_string_create((const uint8_t *)value, strlen(value), &value_str);

	if (exc != DOM_NO_ERR) {
		dom_string_unref(attr_str);
		js_error(J, "error");
		return;
	}

	exc = dom_element_set_attribute(el,
			attr_str, value_str);
	dom_string_unref(attr_str);
	dom_string_unref(value_str);
	if (exc != DOM_NO_ERR) {
		js_pushundefined(J);
		return;
	}
	interpreter->changed = true;
	js_pushundefined(J);
}

static void
mjs_element_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[element object]");
}

void *map_elements;

static void
mjs_element_finalizer(js_State *J, void *priv)
{
	struct mjs_element_private *el_private = (struct mjs_element_private *)priv;

	if (el_private) {
		if (attr_find_in_map(map_elements, el_private)) {
			attr_erase_from_map(map_elements, el_private);

			struct listener *l;

			foreach(l, el_private->listeners) {
				mem_free_set(&l->typ, NULL);
				if (l->fun) js_unref(J, l->fun);
			}
			free_list(el_private->listeners);
			mem_free(el_private);
		}
	}
}

void *map_privates;

void
mjs_push_element(js_State *J, void *node)
{
	struct mjs_element_private *el_private = NULL;

	void *second = attr_find_in_map(map_privates, node);

	if (second) {
		el_private = (struct mjs_element_private *)second;

		if (!attr_find_in_map(map_elements, el_private)) {
			el_private = NULL;
		}
	}

	if (!el_private) {
		el_private = (struct mjs_element_private *)mem_calloc(1, sizeof(*el_private));

		if (!el_private) {
			js_pushnull(J);
			return;
		}
		init_list(el_private->listeners);
		struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
		el_private->interpreter = interpreter;
		el_private->node = node;

		attr_save_in_map(map_privates, node, el_private);
		attr_save_in_map(map_elements, el_private, node);
	}

	js_newobject(J);
	{
		js_copy(J, 0);
		el_private->thisval = js_ref(J);
		js_newuserdata(J, "element", el_private, mjs_element_finalizer);
		addmethod(J, "addEventListener", mjs_element_addEventListener, 3);
		addmethod(J, "appendChild",mjs_element_appendChild, 1);
		addmethod(J, "cloneNode",	mjs_element_cloneNode, 1);
		addmethod(J, "closest",	mjs_element_closest, 1);
		addmethod(J, "contains",	mjs_element_contains, 1);
		addmethod(J, "getAttribute",	mjs_element_getAttribute, 1);
		addmethod(J, "getAttributeNode",	mjs_element_getAttributeNode, 1);
		addmethod(J, "getElementsByTagName",	mjs_element_getElementsByTagName, 1);
		addmethod(J, "hasAttribute",	mjs_element_hasAttribute, 1);
		addmethod(J, "hasAttributes",	mjs_element_hasAttributes, 0);
		addmethod(J, "hasChildNodes",	mjs_element_hasChildNodes, 0);
		addmethod(J, "insertBefore",	mjs_element_insertBefore, 2);
		addmethod(J, "isEqualNode",	mjs_element_isEqualNode, 1);
		addmethod(J, "isSameNode",		mjs_element_isSameNode, 1);
		addmethod(J, "matches",		mjs_element_matches, 1);
		addmethod(J, "querySelector",	mjs_element_querySelector, 1);
		addmethod(J, "querySelectorAll",	mjs_element_querySelectorAll, 1);
		addmethod(J, "remove",		mjs_element_remove, 0);
		addmethod(J, "removeChild",	mjs_element_removeChild, 1);
		addmethod(J, "removeEventListener", mjs_element_removeEventListener, 3);
		addmethod(J, "replaceWith", mjs_element_replaceWith, 1);
		addmethod(J, "setAttribute",	mjs_element_setAttribute, 2);
		addmethod(J, "toString",		mjs_element_toString, 0);

		addproperty(J, "attributes",	mjs_element_get_property_attributes, NULL);
		addproperty(J, "children",	mjs_element_get_property_children, NULL);
		addproperty(J, "childElementCount",	mjs_element_get_property_childElementCount, NULL);
		addproperty(J, "childNodes",	mjs_element_get_property_childNodes, NULL);
		addproperty(J, "className",	mjs_element_get_property_className, mjs_element_set_property_className);
		addproperty(J, "dir",	mjs_element_get_property_dir, mjs_element_set_property_dir);
		addproperty(J, "firstChild",	mjs_element_get_property_firstChild, NULL);
		addproperty(J, "firstElementChild",	mjs_element_get_property_firstElementChild, NULL);
		addproperty(J, "id",	mjs_element_get_property_id, mjs_element_set_property_id);
		addproperty(J, "innerHTML",	mjs_element_get_property_innerHtml, mjs_element_set_property_innerHtml);
		addproperty(J, "innerText",	mjs_element_get_property_innerHtml, mjs_element_set_property_innerText);
		addproperty(J, "lang",	mjs_element_get_property_lang, mjs_element_set_property_lang);
		addproperty(J, "lastChild",	mjs_element_get_property_lastChild, NULL);
		addproperty(J, "lastElementChild",	mjs_element_get_property_lastElementChild, NULL);
		addproperty(J, "nextElementSibling",	mjs_element_get_property_nextElementSibling, NULL);
		addproperty(J, "nextSibling",	mjs_element_get_property_nextSibling, NULL);
		addproperty(J, "nodeName",	mjs_element_get_property_nodeName, NULL);
		addproperty(J, "nodeType",	mjs_element_get_property_nodeType, NULL);
		addproperty(J, "nodeValue",	mjs_element_get_property_nodeValue, NULL);
		addproperty(J, "outerHTML",	mjs_element_get_property_outerHtml, NULL);
		addproperty(J, "ownerDocument",	mjs_element_get_property_ownerDocument, NULL);
		addproperty(J, "parentElement",	mjs_element_get_property_parentElement, NULL);
		addproperty(J, "parentNode",	mjs_element_get_property_parentNode, NULL);
		addproperty(J, "previousElementSibling",	mjs_element_get_property_previousElementSibling, NULL);
		addproperty(J, "previousSibling",	mjs_element_get_property_previousSibling, NULL);
		addproperty(J, "tagName",	mjs_element_get_property_tagName, NULL);
		addproperty(J, "textContent",	mjs_element_get_property_textContent, NULL);
		addproperty(J, "title",	mjs_element_get_property_title, mjs_element_set_property_title);
	}
}

int
mjs_element_init(js_State *J)
{
	mjs_push_element(J, NULL);
	js_defglobal(J, "Element", JS_DONTENUM);

	return 0;
}

void
check_element_event(void *interp, void *elem, const char *event_name, struct term_event *ev)
{
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)interp;
	js_State *J = (js_State *)interpreter->backend_data;
	void *second = attr_find_in_map(map_privates, elem);

	if (!second) {
		return;
	}
	struct mjs_element_private *el_private = (struct mjs_element_private *)second;

	struct listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, event_name)) {
			continue;
		}

		if (ev && ev->ev == EVENT_KBD && (!strcmp(event_name, "keydown") || !strcmp(event_name, "keyup"))) {
			js_getregistry(J, l->fun); /* retrieve the js function from the registry */
			js_getregistry(J, el_private->thisval);
			mjs_push_keyboardEvent(J, ev);
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
