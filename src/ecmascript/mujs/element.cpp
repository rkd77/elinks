/* The MuJS html element objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

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

#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>
#include <libxml++/attributenode.h>
#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <algorithm>
#include <map>
#include <string>

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
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	xmlpp::Element::AttributeList *attrs = new(std::nothrow) xmlpp::Element::AttributeList;

	if (!attrs) {
		js_pushnull(J);
		return;
	}
	*attrs = el->get_attributes();

	mjs_push_attributes(J, attrs);
//	return getAttributes(ctx, attrs);
}

static void
mjs_element_get_property_children(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}

	auto nodes = el->get_children();
	if (nodes.empty()) {
		js_pushnull(J);
		return;
	}

	xmlpp::Node::NodeSet *list = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!list) {
		js_pushnull(J);
		return;
	}

	auto it = nodes.begin();
	auto end = nodes.end();

	for (; it != end; ++it) {
		const auto element = dynamic_cast<xmlpp::Element*>(*it);

		if (element) {
			list->push_back(reinterpret_cast<xmlpp::Node*>(element));
		}
	}

	if (list->empty()) {
		delete list;
		js_pushnull(J);
		return;
	}

	mjs_push_collection(J, list);

//	return getCollection(ctx, list);
}

static void
mjs_element_get_property_childElementCount(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}

	int res = el->get_children().size();

	js_pushnumber(J, res);
}

static void
mjs_element_get_property_childNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}

	xmlpp::Node::NodeList *nodes = new(std::nothrow) xmlpp::Node::NodeList;

	if (!nodes) {
		js_pushnull(J);
		return;
	}

	*nodes = el->get_children();
	if (nodes->empty()) {
		delete nodes;
		js_pushnull(J);
		return;
	}

	mjs_push_nodelist(J, nodes);
//	return getNodeList(ctx, nodes);
}

static void
mjs_element_get_property_className(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = el->get_attribute_value("class");
	js_pushstring(J, v.c_str());
//	JSValue r = JS_NewStringLen(ctx, v.c_str(), v.length());
//	RETURN_JS(r);
}

static void
mjs_element_get_property_dir(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}

	xmlpp::ustring v = el->get_attribute_value("dir");

	if (v != "auto" && v != "ltr" && v != "rtl") {
		v = "";
	}
	js_pushstring(J, v.c_str());
}

static void
mjs_element_get_property_firstChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}

	auto node = el->get_first_child();

	if (!node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
//	return getElement(ctx, node);
}

static void
mjs_element_get_property_firstElementChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}

	auto nodes = el->get_children();
	if (nodes.empty()) {
		js_pushnull(J);
		return;
	}

	auto it = nodes.begin();
	auto end = nodes.end();

	for (; it != end; ++it) {
		auto element = dynamic_cast<xmlpp::Element*>(*it);

		if (element) {
			mjs_push_element(J, element);
			return;
			//return getElement(ctx, element);
		}
	}
	js_pushnull(J);
}

static void
mjs_element_get_property_id(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = el->get_attribute_value("id");
	js_pushstring(J, v.c_str());
}

static void
mjs_element_get_property_lang(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = el->get_attribute_value("lang");
	js_pushstring(J, v.c_str());
}

static void
mjs_element_get_property_lastChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	auto nodes = el->get_children();

	if (nodes.empty()) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, *(nodes.rbegin()));
}

static void
mjs_element_get_property_lastElementChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	auto nodes = el->get_children();

	if (nodes.empty()) {
		js_pushnull(J);
		return;
	}

	auto it = nodes.rbegin();
	auto end = nodes.rend();

	for (; it != end; ++it) {
		auto element = dynamic_cast<xmlpp::Element*>(*it);

		if (element) {
			mjs_push_element(J, element);
			return;
		}
	}
	js_pushnull(J);
}

static void
mjs_element_get_property_nextElementSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	xmlpp::Node *node = el;

	while (true) {
		node = node->get_next_sibling();

		if (!node) {
			js_pushnull(J);
			return;
		}
		xmlpp::Element *next = dynamic_cast<xmlpp::Element*>(node);

		if (next) {
			mjs_push_element(J, next);
			return;
		}
	}
	js_pushnull(J);
}

static void
mjs_element_get_property_nodeName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node *node = static_cast<xmlpp::Node *>(mjs_getprivate(J, 0));

	xmlpp::ustring v;

	if (!node) {
		js_pushstring(J, "");
		return;
	}
	auto el = dynamic_cast<xmlpp::Element*>(node);

	if (el) {
		v = el->get_name();
		std::transform(v.begin(), v.end(), v.begin(), ::toupper);
	} else {
		auto el = dynamic_cast<xmlpp::Attribute*>(node);
		if (el) {
			v = el->get_name();
		} else if (dynamic_cast<xmlpp::TextNode*>(node)) {
			v = "#text";
		} else if (dynamic_cast<xmlpp::CommentNode*>(node)) {
			v = "#comment";
		}
	}
	js_pushstring(J, v.c_str());
}

static void
mjs_element_get_property_nodeType(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node *node = static_cast<xmlpp::Node *>(mjs_getprivate(J, 0));

	if (!node) {
		js_pushnull(J);
		return;
	}

	int ret = 8;

	if (dynamic_cast<const xmlpp::Element*>(node)) {
		ret = 1;
	} else if (dynamic_cast<const xmlpp::Attribute*>(node)) {
		ret = 2;
	} else if (dynamic_cast<const xmlpp::TextNode*>(node)) {
		ret = 3;
	} else if (dynamic_cast<const xmlpp::CommentNode*>(node)) {
		ret = 8;
	}
	js_pushnumber(J, ret);
}

static void
mjs_element_get_property_nodeValue(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node *node = static_cast<xmlpp::Node *>(mjs_getprivate(J, 0));

	if (!node) {
		js_pushnull(J);
		return;
	}

	if (dynamic_cast<const xmlpp::Element*>(node)) {
		js_pushnull(J);
		return;
	}

	auto el = dynamic_cast<const xmlpp::Attribute*>(node);

	if (el) {
		xmlpp::ustring v = el->get_value();
		js_pushstring(J, v.c_str());
		return;
	}

	auto el2 = dynamic_cast<const xmlpp::TextNode*>(node);

	if (el2) {
		xmlpp::ustring v = el2->get_content();
		js_pushstring(J, v.c_str());
		return;
	}

	auto el3 = dynamic_cast<const xmlpp::CommentNode*>(node);

	if (el3) {
		xmlpp::ustring v = el3->get_content();
		js_pushstring(J, v.c_str());
		return;
	}
	js_pushundefined(J);
}

static void
mjs_element_get_property_nextSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}

	auto node = el->get_next_sibling();

	if (!node) {
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
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}

	auto node = dynamic_cast<xmlpp::Element*>(el->get_parent());

	if (!node) {
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
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	auto node = el->get_parent();

	if (!node) {
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
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	xmlpp::Node *node = el;

	while (true) {
		node = node->get_previous_sibling();

		if (!node) {
			js_pushnull(J);
			return;
		}
		xmlpp::Element *next = dynamic_cast<xmlpp::Element*>(node);

		if (next) {
			mjs_push_element(J, next);
			return;
		}
	}
	js_pushnull(J);
}

static void
mjs_element_get_property_previousSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	auto node = el->get_previous_sibling();

	if (!node) {
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
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = el->get_name();
	std::transform(v.begin(), v.end(), v.begin(), ::toupper);
	js_pushstring(J, v.c_str());
}

static void
mjs_element_get_property_title(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = el->get_attribute_value("title");
	js_pushstring(J, v.c_str());
}

static void
dump_element(struct string *buf, xmlpp::Element *element, bool toSort = false)
{
	add_char_to_string(buf, '<');
	add_to_string(buf, element->get_name().c_str());
	auto attrs = element->get_attributes();
	if (toSort) {
		attrs.sort([](const xmlpp::Attribute *a1, const xmlpp::Attribute *a2)
		{
			if (a1->get_name() == a2->get_name()) {
				return a1->get_value() < a2->get_value();
			}
			return a1->get_name() < a2->get_name();
		});
	}
	auto it = attrs.begin();
	auto end = attrs.end();
	for (;it != end; ++it) {
		add_char_to_string(buf, ' ');
		add_to_string(buf, (*it)->get_name().c_str());
		add_char_to_string(buf, '=');
		add_char_to_string(buf, '"');
		add_to_string(buf, (*it)->get_value().c_str());
		add_char_to_string(buf, '"');
	}
	add_char_to_string(buf, '>');
}

void
walk_tree(struct string *buf, void *nod, bool start, bool toSortAttrs)
{
	xmlpp::Node *node = static_cast<xmlpp::Node *>(nod);

	if (!start) {
		const auto textNode = dynamic_cast<const xmlpp::ContentNode*>(node);

		if (textNode) {
			add_bytes_to_string(buf, textNode->get_content().c_str(), textNode->get_content().length());
		} else {
			auto element = dynamic_cast<xmlpp::Element*>(node);

			if (element) {
				dump_element(buf, element, toSortAttrs);
			}
		}
	}

	auto childs = node->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		walk_tree(buf, *it, false, toSortAttrs);
	}

	if (!start) {
		const auto element = dynamic_cast<const xmlpp::Element*>(node);
		if (element) {
			add_to_string(buf, "</");
			add_to_string(buf, element->get_name().c_str());
			add_char_to_string(buf, '>');
		}
	}
}

static void
walk_tree_content(struct string *buf, xmlpp::Node *node)
{
	const auto nodeText = dynamic_cast<const xmlpp::TextNode*>(node);

	if (nodeText) {
		add_bytes_to_string(buf, nodeText->get_content().c_str(), nodeText->get_content().length());
	}

	auto childs = node->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		walk_tree_content(buf, *it);
	}
}

static void
mjs_element_get_property_innerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct string buf;
	if (!init_string(&buf)) {
		js_pushnull(J);
		return;
	}
	walk_tree(&buf, el);
	js_pushstring(J, buf.source);
	done_string(&buf);
}

static void
mjs_element_get_property_outerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct string buf;
	if (!init_string(&buf)) {
		js_pushnull(J);
		return;
	}
	walk_tree(&buf, el, false);
	js_pushstring(J, buf.source);
	done_string(&buf);
}

static void
mjs_element_get_property_textContent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct string buf;
	if (!init_string(&buf)) {
		js_pushnull(J);
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
	const char *val = js_tostring(J, 1);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	xmlpp::ustring value = val;
	el->set_attribute("class", value);
	interpreter->changed = true;
	js_pushundefined(J);
}

static void
mjs_element_set_property_dir(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *val = js_tostring(J, 1);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	xmlpp::ustring value = val;

	if (value == "ltr" || value == "rtl" || value == "auto") {
		el->set_attribute("dir", value);
		interpreter->changed = true;
	}
	js_pushundefined(J);
}

static void
mjs_element_set_property_id(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *val = js_tostring(J, 1);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	xmlpp::ustring value = val;
	el->set_attribute("id", value);
	interpreter->changed = true;
	js_pushundefined(J);
}

static void
mjs_element_set_property_innerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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
	xmlpp::ustring text = "<root>";
	text += val;
	text += "</root>";

	xmlDoc* doc = htmlReadDoc((xmlChar*)text.c_str(), NULL, "utf-8", HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	// Encapsulate raw libxml document in a libxml++ wrapper
	xmlpp::Document doc1(doc);

	auto root = doc1.get_root_node();
	auto root1 = root->find("//root")[0];
	auto children2 = root1->get_children();
	auto it2 = children2.begin();
	auto end2 = children2.end();
	for (; it2 != end2; ++it2) {
		el->import_node(*it2);
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

	js_pushundefined(J);
}

static void
mjs_element_set_property_lang(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	xmlpp::ustring value = str;
	el->set_attribute("lang", value);
	interpreter->changed = true;

	js_pushundefined(J);
}

static void
mjs_element_set_property_title(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	xmlpp::ustring value = str;
	el->set_attribute("title", value);
	interpreter->changed = true;

	js_pushundefined(J);
}

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

static void
check_contains(xmlpp::Node *node, xmlpp::Node *searched, bool *result_set, bool *result)
{
	if (*result_set) {
		return;
	}

	auto childs = node->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		if (*it == searched) {
			*result_set = true;
			*result = true;
			return;
		}
		check_contains(*it, searched, result_set, result);
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
		js_pushnull(J);
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_pushnull(J);
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
		js_pushnull(J);
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_pushnull(J);
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
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	xmlpp::Node *el2 = static_cast<xmlpp::Node *>(mjs_getprivate(J, 1));
	el2 = el->import_node(el2);
	interpreter->changed = true;

	mjs_push_element(J, el2);
}

static void
mjs_element_cloneNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

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

	try {
		xmlpp::Node *node2 = node->import_node(el, js_toboolean(J, 1));

		if (!node2) {
			js_pushnull(J);
			return;
		}

		mjs_push_element(J, node2);
		return;
	} catch (xmlpp::exception &e) {
		js_pushnull(J);
		return;
	}
}

static bool
isAncestor(xmlpp::Element *el, xmlpp::Node *node)
{
	while (node) {
		if (el == node) {
			return true;
		}
		node = node->get_parent();
	}

	return false;
}

static void
mjs_element_closest(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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
	js_pushnull(J);
}

static void
mjs_element_contains(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	xmlpp::Element *el2 = static_cast<xmlpp::Element *>(mjs_getprivate(J, 1));

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
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	const char *str = js_tostring(J, 1);
	xmlpp::ustring v = str;
	xmlpp::Attribute *attr = el->get_attribute(v);

	if (!attr) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring val = attr->get_value();
	js_pushstring(J, val.c_str());
}

static void
mjs_element_getAttributeNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}

	if (!str) {
		js_pushnull(J);
		return;
	}
	xmlpp::ustring v = str;
	xmlpp::Attribute *attr = el->get_attribute(v);

	mjs_push_attr(J, attr);
}

static void
mjs_element_getElementsByTagName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}

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
	*elements = el->find(xpath);
	mjs_push_collection(J, elements);
}

static void
mjs_element_hasAttribute(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	const char *str = js_tostring(J, 1);
	xmlpp::ustring v = str;
	xmlpp::Attribute *attr = el->get_attribute(v);

	js_pushboolean(J, (bool)attr);
}

static void
mjs_element_hasAttributes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	auto attrs = el->get_attributes();

	js_pushboolean(J, (bool)attrs.size());
}

static void
mjs_element_hasChildNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	auto children = el->get_children();

	js_pushboolean(J, (bool)children.size());
}

static void
mjs_element_insertBefore(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	xmlpp::Node *next_sibling = static_cast<xmlpp::Node *>(mjs_getprivate(J, 2));

	if (!next_sibling) {
		js_pushnull(J);
		return;
	}

	xmlpp::Node *child = static_cast<xmlpp::Node *>(mjs_getprivate(J, 1));
	auto node = xmlAddPrevSibling(next_sibling->cobj(), child->cobj());
	auto res = el_add_child_element_common(child->cobj(), node);

	interpreter->changed = true;

	mjs_push_element(J, res);
}

static void
mjs_element_isEqualNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}

	xmlpp::Element *el2 = static_cast<xmlpp::Element *>(mjs_getprivate(J, 1));

	struct string first;
	struct string second;

	if (!init_string(&first)) {
		js_pushnull(J);
		return;
	}
	if (!init_string(&second)) {
		done_string(&first);
		js_pushnull(J);
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
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	xmlpp::Element *el2 = static_cast<xmlpp::Element *>(mjs_getprivate(J, 1));

	js_pushboolean(J, (el == el2));
}

static void
mjs_element_matches(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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
	js_pushboolean(J, 0);
}

static void
mjs_element_querySelector(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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
	js_pushnull(J);
}

static void
mjs_element_querySelectorAll(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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
}

static void
mjs_element_remove(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	xmlpp::Node::remove_node(el);
	interpreter->changed = true;

	js_pushundefined(J);
}

static void
mjs_element_removeChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));
	xmlpp::Element *el2 = static_cast<xmlpp::Element *>(mjs_getprivate(J, 1));

	if (!el || !el2) {
		js_pushnull(J);
		return;
	}
	auto children = el->get_children();
	auto it = children.begin();
	auto end = children.end();

	for (;it != end; ++it) {
		if (*it == el2) {
			xmlpp::Node::remove_node(el2);
			interpreter->changed = true;

			mjs_push_element(J, el2);
			return;
		}
	}
	js_pushnull(J);
}

static void
mjs_element_replaceWith(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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

	js_pushundefined(J);
}

static void
mjs_element_setAttribute(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	xmlpp::Element *el = static_cast<xmlpp::Element *>(mjs_getprivate(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *attr_c = js_tostring(J, 1);
	const char *value_c = js_tostring(J, 2);

	xmlpp::ustring attr = attr_c;
	xmlpp::ustring value = value_c;
	el->set_attribute(attr, value);
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

static std::map<void *, void *> map_elements;

static void
mjs_element_finalizer(js_State *J, void *priv)
{
	struct mjs_element_private *el_private = (struct mjs_element_private *)priv;

	if (el_private) {
		if (map_elements.find(el_private) != map_elements.end()) {
			map_elements.erase(el_private);

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

static std::map<void *, struct mjs_element_private *> map_privates;

void
mjs_push_element(js_State *J, void *node)
{
	struct mjs_element_private *el_private = NULL;
	auto node_find = map_privates.find(node);

	if (node_find != map_privates.end()) {
		el_private = (struct mjs_element_private *)node_find->second;
		if (map_elements.find(el_private) == map_elements.end()) {
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
		map_privates[node] = el_private;
		map_elements[el_private] = node;
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
check_element_event(void *elem, const char *event_name, struct term_event *ev)
{
	auto el = map_privates.find(elem);

	if (el == map_privates.end()) {
		return;
	}
	struct mjs_element_private *el_private = el->second;
	struct ecmascript_interpreter *interpreter = el_private->interpreter;
	js_State *J = (js_State *)interpreter->backend_data;

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
