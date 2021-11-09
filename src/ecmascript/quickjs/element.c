/* The QuickJS html element objects implementation. */

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
#include "ecmascript/quickjs/attr.h"
#include "ecmascript/quickjs/attributes.h"
#include "ecmascript/quickjs/collection.h"
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
#include <libxml++/attributenode.h>
#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <algorithm>
#include <map>
#include <string>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_element_class_id;

static JSValue
js_element_get_property_attributes(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}

	xmlpp::Element::AttributeList *attrs = new xmlpp::Element::AttributeList;

	*attrs = el->get_attributes();

	return getAttributes(ctx, attrs);
}

static JSValue
js_element_get_property_children(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}

	auto nodes = el->get_children();
	if (nodes.empty()) {
		return JS_NULL;
	}

	xmlpp::Node::NodeSet *list = new xmlpp::Node::NodeSet;

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
		return JS_NULL;
	}

	return getCollection(ctx, list);
}

static JSValue
js_element_get_property_childElementCount(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}

	int res = el->get_children().size();

	return JS_NewUint32(ctx, res);
}

static JSValue
js_element_get_property_childNodes(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}

	xmlpp::Node::NodeList *nodes = new xmlpp::Node::NodeList;

	*nodes = el->get_children();
	if (nodes->empty()) {
		delete nodes;
		return JS_NULL;
	}

	return getNodeList(ctx, nodes);
}

static JSValue
js_element_get_property_className(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	xmlpp::ustring v = el->get_attribute_value("class");

	return JS_NewStringLen(ctx, v.c_str(), v.length());
}

static JSValue
js_element_get_property_dir(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}

	xmlpp::ustring v = el->get_attribute_value("dir");

	if (v != "auto" && v != "ltr" && v != "rtl") {
		v = "";
	}
	return JS_NewStringLen(ctx, v.c_str(), v.length());
}

static JSValue
js_element_get_property_firstChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}

	auto node = el->get_first_child();

	if (!node) {
		return JS_NULL;
	}

	return getElement(ctx, node);
}

static JSValue
js_element_get_property_firstElementChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}

	auto nodes = el->get_children();
	if (nodes.empty()) {
		return JS_NULL;
	}

	auto it = nodes.begin();
	auto end = nodes.end();

	for (; it != end; ++it) {
		const auto element = dynamic_cast<const xmlpp::Element*>(*it);

		if (element) {
			return getElement(ctx, element);
		}
	}
	return JS_NULL;
}

static JSValue
js_element_get_property_id(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	xmlpp::ustring v = el->get_attribute_value("id");

	return JS_NewStringLen(ctx, v.c_str(), v.length());
}

static JSValue
js_element_get_property_lang(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	xmlpp::ustring v = el->get_attribute_value("lang");

	return JS_NewStringLen(ctx, v.c_str(), v.length());
}

static JSValue
js_element_get_property_lastChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	auto nodes = el->get_children();

	if (nodes.empty()) {
		return JS_NULL;
	}

	return getElement(ctx, *(nodes.rbegin()));
}

static JSValue
js_element_get_property_lastElementChild(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	auto nodes = el->get_children();

	if (nodes.empty()) {
		return JS_NULL;
	}

	auto it = nodes.rbegin();
	auto end = nodes.rend();

	for (; it != end; ++it) {
		const auto element = dynamic_cast<const xmlpp::Element*>(*it);

		if (element) {
			return getElement(ctx, element);
		}
	}

	return JS_NULL;
}

static JSValue
js_element_get_property_nextElementSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	xmlpp::Node *node = el;

	while (true) {
		node = node->get_next_sibling();

		if (!node) {
			return JS_NULL;
		}
		xmlpp::Element *next = dynamic_cast<const xmlpp::Element*>(node);

		if (next) {
			return getElement(ctx, next);
		}
	}

	return JS_NULL;
}

static JSValue
js_element_get_property_nodeName(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node *node = JS_GetOpaque(this_val, js_element_class_id);

	xmlpp::ustring v;

	if (!node) {
		return JS_NewStringLen(ctx, "", 0);
	}
	auto el = dynamic_cast<const xmlpp::Element*>(node);

	if (el) {
		v = el->get_name();
		std::transform(v.begin(), v.end(), v.begin(), ::toupper);
	} else {
		auto el = dynamic_cast<const xmlpp::Attribute*>(node);
		if (el) {
			v = el->get_name();
		} else if (dynamic_cast<const xmlpp::TextNode*>(node)) {
			v = "#text";
		} else if (dynamic_cast<const xmlpp::CommentNode*>(node)) {
			v = "#comment";
		}
	}

	return JS_NewStringLen(ctx, v.c_str(), v.length());
}

static JSValue
js_element_get_property_nodeType(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node *node = JS_GetOpaque(this_val, js_element_class_id);

	if (!node) {
		return JS_NULL;
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
	return JS_NewUint32(ctx, ret);
}

static JSValue
js_element_get_property_nodeValue(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node *node = JS_GetOpaque(this_val, js_element_class_id);

	if (!node) {
		return JS_NULL;
	}

	if (dynamic_cast<const xmlpp::Element*>(node)) {
		return JS_NULL;
	}

	auto el = dynamic_cast<const xmlpp::Attribute*>(node);

	if (el) {
		xmlpp::ustring v = el->get_value();

		return JS_NewStringLen(ctx, v.c_str(), v.length());
	}

	auto el2 = dynamic_cast<const xmlpp::TextNode*>(node);

	if (el2) {
		xmlpp::ustring v = el2->get_content();

		return JS_NewStringLen(ctx, v.c_str(), v.length());
	}

	auto el3 = dynamic_cast<const xmlpp::CommentNode*>(node);

	if (el3) {
		xmlpp::ustring v = el3->get_content();

		return JS_NewStringLen(ctx, v.c_str(), v.length());
	}

	return JS_UNDEFINED;
}

static JSValue
js_element_get_property_nextSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}

	auto node = el->get_next_sibling();

	if (!node) {
		return JS_NULL;
	}

	return getElement(ctx, node);
}

static JSValue
js_element_get_property_ownerDocument(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);

	return JS_DupValue(ctx, interpreter->document_obj);
}

static JSValue
js_element_get_property_parentElement(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}

	auto node = dynamic_cast<xmlpp::Element*>(el->get_parent());

	if (!node) {
		return JS_NULL;
	}

	return getElement(ctx, node);
}

static JSValue
js_element_get_property_parentNode(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	auto node = el->get_parent();

	if (!node) {
		return JS_NULL;
	}

	return getElement(ctx, node);
}

static JSValue
js_element_get_property_previousElementSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	xmlpp::Node *node = el;

	while (true) {
		node = node->get_previous_sibling();

		if (!node) {
			return JS_NULL;
		}
		xmlpp::Element *next = dynamic_cast<const xmlpp::Element*>(node);

		if (next) {
			return getElement(ctx, next);
		}
	}

	return JS_NULL;
}

static JSValue
js_element_get_property_previousSibling(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	auto node = el->get_previous_sibling();

	if (!node) {
		return JS_NULL;
	}

	return getElement(ctx, node);
}

static JSValue
js_element_get_property_tagName(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	xmlpp::ustring v = el->get_name();
	std::transform(v.begin(), v.end(), v.begin(), ::toupper);

	return JS_NewStringLen(ctx, v.c_str(), v.length());
}

static JSValue
js_element_get_property_title(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	xmlpp::ustring v = el->get_attribute_value("title");

	return JS_NewStringLen(ctx, v.c_str(), v.length());
}

static int was_el = 0;

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
walk_tree(struct string *buf, void *nod, bool start = true, bool toSortAttrs = false)
{
	xmlpp::Node *node = nod;

	if (!start) {
		const auto textNode = dynamic_cast<const xmlpp::ContentNode*>(node);

		if (textNode) {
			add_to_string(buf, textNode->get_content().c_str());
		} else {
			const auto element = dynamic_cast<const xmlpp::Element*>(node);

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
		add_to_string(buf, nodeText->get_content().c_str());
	}

	auto childs = node->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		walk_tree_content(buf, *it);
	}
}

static JSValue
js_element_get_property_innerHtml(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	struct string buf;
	init_string(&buf);
	walk_tree(&buf, el);
	JSValue ret = JS_NewStringLen(ctx, buf.source, buf.length);
	done_string(&buf);

	return ret;
}

static JSValue
js_element_get_property_outerHtml(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	struct string buf;
	init_string(&buf);
	walk_tree(&buf, el, false);
	JSValue ret = JS_NewStringLen(ctx, buf.source, buf.length);
	done_string(&buf);

	return ret;
}

static JSValue
js_element_get_property_textContent(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	struct string buf;
	init_string(&buf);
	walk_tree_content(&buf, el);
	JSValue ret = JS_NewStringLen(ctx, buf.source, buf.length);
	done_string(&buf);

	return ret;
}

static JSValue
js_element_set_property_className(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	assert(interpreter);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring value = str;
	el->set_attribute("class", value);
	interpreter->changed = true;
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_dir(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	assert(interpreter);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring value = str;

	if (value == "ltr" || value == "rtl" || value == "auto") {
		el->set_attribute("dir", value);
		interpreter->changed = true;
	}
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_id(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	assert(interpreter);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring value = str;
	el->set_attribute("id", value);
	interpreter->changed = true;
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_innerHtml(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}
	auto children = el->get_children();
	auto it = children.begin();
	auto end = children.end();

	for (;it != end; ++it) {
		xmlpp::Node::remove_node(*it);
	}
	xmlpp::ustring text = "<root>";
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	text += str;
	text += "</root>";
	JS_FreeCString(ctx, str);

	xmlDoc* doc = htmlReadDoc((xmlChar*)text.c_str(), NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
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

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_innerText(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}
	auto children = el->get_children();
	auto it = children.begin();
	auto end = children.end();

	for (;it != end; ++it) {
		xmlpp::Node::remove_node(*it);
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	el->add_child_text(str);
	interpreter->changed = true;
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_lang(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring value = str;
	el->set_attribute("lang", value);
	interpreter->changed = true;
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_outerHtml(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);
// TODO
	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_textContent(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);
// TODO
	return JS_UNDEFINED;
}

static JSValue
js_element_set_property_title(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring value = str;
	el->set_attribute("title", value);
	interpreter->changed = true;
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
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

static JSValue
js_element_appendChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (argc != 1) {
		return JS_NULL;
	}

	if (!el) {
		return JS_NULL;
	}
	xmlpp::Node *el2 = JS_GetOpaque(argv[0], js_element_class_id);
	el->import_node(el2);
	interpreter->changed = true;

	return getElement(ctx, el2);
}

static JSValue
js_element_cloneNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_NULL;
	}
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	xmlpp::Document *doc2 = document->dom;
	xmlDoc *docu = doc2->cobj();
	xmlNode *xmlnode = xmlNewDocFragment(docu);

	if (!xmlnode) {
		return JS_NULL;
	}
	xmlpp::Node *node = new xmlpp::Node(xmlnode);

	try {
		xmlpp::Node *node2 = node->import_node(el, JS_ToBool(ctx, argv[0]));

		if (!node2) {
			return JS_NULL;
		}

		return getElement(ctx, node2);
	} catch (xmlpp::exception e) {
		return JS_NULL;
	}
}

static JSValue
js_element_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_FALSE;
	}
	xmlpp::Element *el2 = JS_GetOpaque(argv[0], js_element_class_id);

	if (!el2) {
		return JS_FALSE;
	}

	bool result_set = false;
	bool result = false;

	check_contains(el, el2, &result_set, &result);

	return JS_NewBool(ctx, result);
}

static JSValue
js_element_getAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_FALSE;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_NULL;
	}
	xmlpp::ustring v = str;
	xmlpp::Attribute *attr = el->get_attribute(v);
	JS_FreeCString(ctx, str);

	if (!attr) {
		return JS_NULL;
	}
	xmlpp::ustring val = attr->get_value();

	return JS_NewStringLen(ctx, val.c_str(), val.length());
}

static JSValue
js_element_getAttributeNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_NULL;
	}
	xmlpp::ustring v = str;
	xmlpp::Attribute *attr = el->get_attribute(v);
	JS_FreeCString(ctx, str);

	return getAttr(ctx, attr);
}

static JSValue
js_element_hasAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_FALSE;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_NULL;
	}
	xmlpp::ustring v = str;
	xmlpp::Attribute *attr = el->get_attribute(v);
	JS_FreeCString(ctx, str);

	return JS_NewBool(ctx, (bool)attr);
}

static JSValue
js_element_hasAttributes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 0) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_FALSE;
	}
	auto attrs = el->get_attributes();

	return JS_NewBool(ctx, (bool)attrs.size());
}

static JSValue
js_element_hasChildNodes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 0) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_FALSE;
	}
	auto children = el->get_children();

	return JS_NewBool(ctx, (bool)children.size());
}

static JSValue
js_element_insertBefore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 2) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}

	JSValue next_sibling1 = argv[1];
	JSValue child1 = argv[0];

	xmlpp::Node *next_sibling = JS_GetOpaque(next_sibling1, js_element_class_id);

	if (!next_sibling) {
		return JS_NULL;
	}

	xmlpp::Node *child = JS_GetOpaque(child1, js_element_class_id);
	auto node = xmlAddPrevSibling(next_sibling->cobj(), child->cobj());
	auto res = el_add_child_element_common(child->cobj(), node);

	interpreter->changed = true;

	return getElement(ctx, res);
}

static JSValue
js_element_isEqualNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_FALSE;
	}

	JSValue node = argv[0];
	xmlpp::Element *el2 = JS_GetOpaque(node, js_element_class_id);

	struct string first;
	struct string second;

	init_string(&first);
	init_string(&second);

	walk_tree(&first, el, false, true);
	walk_tree(&second, el2, false, true);

	bool ret = !strcmp(first.source, second.source);

	done_string(&first);
	done_string(&second);

	return JS_NewBool(ctx, ret);
}

static JSValue
js_element_isSameNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_FALSE;
	}
	JSValue node = argv[0];
	xmlpp::Element *el2 = JS_GetOpaque(node, js_element_class_id);

	return JS_NewBool(ctx, (el == el2));
}

static JSValue
js_element_querySelector(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_UNDEFINED;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_FALSE;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring css = str;
	xmlpp::ustring xpath = css2xpath(css);
	JS_FreeCString(ctx, str);
	xmlpp::Node::NodeSet elements;

	try {
		elements = el->find(xpath);
	} catch (xmlpp::exception) {
		return JS_NULL;
	}

	if (elements.size() == 0) {
		return JS_NULL;
	}
	auto node = elements[0];

	return getElement(ctx, node);
}

static JSValue
js_element_querySelectorAll(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_FALSE;
	}
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_FALSE;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	xmlpp::ustring css = str;
	xmlpp::ustring xpath = css2xpath(css);
	JS_FreeCString(ctx, str);
	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	try {
		*elements = el->find(xpath);
	} catch (xmlpp::exception) {
	}

	return getCollection(ctx, elements);
}

static JSValue
js_element_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 0) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}

	xmlpp::Node::remove_node(el);
	interpreter->changed = true;

	return JS_UNDEFINED;
}

static JSValue
js_element_removeChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el || !JS_IsObject(argv[0])) {
		return JS_NULL;
	}
	JSValue node = argv[0];
	auto children = el->get_children();
	auto it = children.begin();
	auto end = children.end();
	xmlpp::Element *el2 = JS_GetOpaque(node, js_element_class_id);

	for (;it != end; ++it) {
		if (*it == el2) {
			xmlpp::Node::remove_node(el2);
			interpreter->changed = true;

			return getElement(ctx, el2);
		}
	}

	return JS_NULL;
}

static JSValue
js_element_setAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 2) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	xmlpp::Element *el = JS_GetOpaque(this_val, js_element_class_id);

	if (!el) {
		return JS_UNDEFINED;
	}
	const char *attr_c;
	const char *value_c;
	size_t len_attr, len_value;
	attr_c = JS_ToCStringLen(ctx, &len_attr, argv[0]);

	if (!attr_c) {
		return JS_EXCEPTION;
	}
	value_c = JS_ToCStringLen(ctx, &len_value, argv[1]);

	if (!value_c) {
		JS_FreeCString(ctx, attr_c);
		return JS_EXCEPTION;
	}

	xmlpp::ustring attr = attr_c;
	xmlpp::ustring value = value_c;
	el->set_attribute(attr, value);
	interpreter->changed = true;
	JS_FreeCString(ctx, attr_c);
	JS_FreeCString(ctx, value_c);

	return JS_UNDEFINED;
}


static const JSCFunctionListEntry js_element_proto_funcs[] = {
	JS_CGETSET_DEF("attributes",	js_element_get_property_attributes, nullptr),
	JS_CGETSET_DEF("children",	js_element_get_property_children, nullptr),
	JS_CGETSET_DEF("childElementCount",	js_element_get_property_childElementCount, nullptr),
	JS_CGETSET_DEF("childNodes",	js_element_get_property_childNodes, nullptr),
	JS_CGETSET_DEF("className",	js_element_get_property_className, js_element_set_property_className),
	JS_CGETSET_DEF("dir",	js_element_get_property_dir, js_element_set_property_dir),
	JS_CGETSET_DEF("firstChild",	js_element_get_property_firstChild, nullptr),
	JS_CGETSET_DEF("firstElementChild",	js_element_get_property_firstElementChild, nullptr),
	JS_CGETSET_DEF("id",	js_element_get_property_id, js_element_set_property_id),
	JS_CGETSET_DEF("innerHTML",	js_element_get_property_innerHtml, js_element_set_property_innerHtml),
	JS_CGETSET_DEF("innerText",	js_element_get_property_innerHtml, js_element_set_property_innerText),
	JS_CGETSET_DEF("lang",	js_element_get_property_lang, js_element_set_property_lang),
	JS_CGETSET_DEF("lastChild",	js_element_get_property_lastChild, nullptr),
	JS_CGETSET_DEF("lastElementChild",	js_element_get_property_lastElementChild, nullptr),
	JS_CGETSET_DEF("nextElementSibling",	js_element_get_property_nextElementSibling, nullptr),
	JS_CGETSET_DEF("nextSibling",	js_element_get_property_nextSibling, nullptr),
	JS_CGETSET_DEF("nodeName",	js_element_get_property_nodeName, nullptr),
	JS_CGETSET_DEF("nodeType",	js_element_get_property_nodeType, nullptr),
	JS_CGETSET_DEF("nodeValue",	js_element_get_property_nodeValue, nullptr),
	JS_CGETSET_DEF("outerHTML",	js_element_get_property_outerHtml, js_element_set_property_outerHtml),
	JS_CGETSET_DEF("ownerDocument",	js_element_get_property_ownerDocument, nullptr),
	JS_CGETSET_DEF("parentElement",	js_element_get_property_parentElement, nullptr),
	JS_CGETSET_DEF("parentNode",	js_element_get_property_parentNode, nullptr),
	JS_CGETSET_DEF("previousElementSibling",	js_element_get_property_previousElementSibling, nullptr),
	JS_CGETSET_DEF("previousSibling",	js_element_get_property_previousSibling, nullptr),
	JS_CGETSET_DEF("tagName",	js_element_get_property_tagName, nullptr),
	JS_CGETSET_DEF("textContent",	js_element_get_property_textContent, js_element_set_property_textContent),
	JS_CGETSET_DEF("title",	js_element_get_property_title, js_element_set_property_title),
	JS_CFUNC_DEF("appendChild",	1, js_element_appendChild),
	JS_CFUNC_DEF("cloneNode",	1, js_element_cloneNode),
	JS_CFUNC_DEF("contains",	1, js_element_contains),
	JS_CFUNC_DEF("getAttribute",	1,	js_element_getAttribute),
	JS_CFUNC_DEF("getAttributeNode",1,	js_element_getAttributeNode),
	JS_CFUNC_DEF("hasAttribute",	1,	js_element_hasAttribute),
	JS_CFUNC_DEF("hasAttributes",	0,	js_element_hasAttributes),
	JS_CFUNC_DEF("hasChildNodes",	0,	js_element_hasChildNodes),
	JS_CFUNC_DEF("insertBefore",	2,	js_element_insertBefore),
	JS_CFUNC_DEF("isEqualNode",	1, js_element_isEqualNode),
	JS_CFUNC_DEF("isSameNode",	1,		js_element_isSameNode),
	JS_CFUNC_DEF("querySelector",1,		js_element_querySelector),
	JS_CFUNC_DEF("querySelectorAll",1,		js_element_querySelectorAll),
	JS_CFUNC_DEF("remove",	0,	js_element_remove),
	JS_CFUNC_DEF("removeChild",1,	js_element_removeChild),
	JS_CFUNC_DEF("setAttribute",2,	js_element_setAttribute),
};

static std::map<void *, JSValueConst> map_elements;

void js_element_finalizer(JSRuntime *rt, JSValue val)
{
	void *node = JS_GetOpaque(val, js_element_class_id);

	map_elements.erase(node);
}

static JSClassDef js_element_class = {
	"element",
	js_element_finalizer
};

static JSValue
js_element_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_element_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	return obj;

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_element_init(JSContext *ctx, JSValue global_obj)
{
	JSValue element_proto, element_class;

	/* create the element class */
	JS_NewClassID(&js_element_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_element_class_id, &js_element_class);

	element_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, element_proto, js_element_proto_funcs, countof(js_element_proto_funcs));

	element_class = JS_NewCFunction2(ctx, js_element_ctor, "element", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, element_class, element_proto);
	JS_SetClassProto(ctx, js_element_class_id, element_proto);

	JS_SetPropertyStr(ctx, global_obj, "element", element_proto);
	return 0;
}

JSValue
getElement(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	auto node_find = map_elements.find(node);

	if (node_find != map_elements.end()) {
		return JS_DupValue(ctx, node_find->second);
	}
	static int initialized;
	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_element_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_element_class_id, &js_element_class);
		initialized = 1;
	}

	JSValue element_obj = JS_NewObjectClass(ctx, js_element_class_id);

	JS_SetPropertyFunctionList(ctx, element_obj, js_element_proto_funcs, countof(js_element_proto_funcs));
//	JSValue element_class = JS_NewCFunction2(ctx, js_element_ctor, "element", 0, JS_CFUNC_constructor, 0);
//	JS_SetConstructor(ctx, element_class, element_obj);
	JS_SetClassProto(ctx, js_element_class_id, element_obj);
	JS_SetOpaque(element_obj, node);

	map_elements[node] = element_obj;

	return JS_DupValue(ctx, element_obj);
}
