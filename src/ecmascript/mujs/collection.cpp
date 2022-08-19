/* The MuJS html collection object implementation. */

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
#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/collection.h"
#include "ecmascript/mujs/element.h"
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

static std::map<void *, void *> map_collections;
static std::map<void *, void *> map_rev_collections;

static void
mjs_htmlCollection_get_property_length(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeSet *ns = static_cast<xmlpp::Node::NodeSet *>(js_touserdata(J, 0, "collection"));

	if (!ns) {
		js_pushnumber(J, 0);
		return;
	}
	js_pushnumber(J, ns->size());
}

static void
mjs_push_htmlCollection_item2(js_State *J, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeSet *ns = static_cast<xmlpp::Node::NodeSet *>(js_touserdata(J, 0, "collection"));

	if (!ns) {
		js_pushundefined(J);
		return;
	}

	xmlpp::Element *element;

	try {
		element = dynamic_cast<xmlpp::Element *>(ns->at(idx));
	} catch (std::out_of_range &e) {
		js_pushundefined(J);
		return;
	}

	if (!element) {
		js_pushundefined(J);
		return;
	}
	mjs_push_element(J, element);
}

static void
mjs_htmlCollection_item(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int index = js_toint32(J, 1);

	mjs_push_htmlCollection_item2(J, index);
}

static void
mjs_push_htmlCollection_namedItem2(js_State *J, const char *str)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeSet *ns = static_cast<xmlpp::Node::NodeSet *>(js_touserdata(J, 0, "collection"));

	if (!ns) {
		js_pushundefined(J);
		return;
	}

	xmlpp::ustring name = str;

	auto it = ns->begin();
	auto end = ns->end();

	for (; it != end; ++it) {
		auto element = dynamic_cast<xmlpp::Element*>(*it);

		if (!element) {
			continue;
		}

		if (name == element->get_attribute_value("id")
		|| name == element->get_attribute_value("name")) {
			mjs_push_element(J, element);
			return;
		}
	}
	js_pushundefined(J);
}

static void
mjs_htmlCollection_namedItem(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	mjs_push_htmlCollection_namedItem2(J, str);
}

static void
mjs_htmlCollection_set_items(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int counter = 0;

	xmlpp::Node::NodeSet *ns = static_cast<xmlpp::Node::NodeSet *>(node);

	if (!ns) {
		return;
	}

	xmlpp::Element *element;

	while (1) {
		try {
			element = dynamic_cast<xmlpp::Element *>(ns->at(counter));
		} catch (std::out_of_range &e) {
			return;
		}

		if (!element) {
			return;
		}
		mjs_push_element(J, element);
		js_setindex(J, -2, counter);

		xmlpp::ustring name = element->get_attribute_value("id");
		if (name == "") {
			name = element->get_attribute_value("name");
		}
		if (name != "" && name != "item" && name != "namedItem") {
			mjs_push_element(J, element);
			js_setproperty(J, -2, name.c_str());
		}
		counter++;
	}
}

static void
mjs_htmlCollection_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[htmlCollection object]");
}

static void
mjs_htmlCollection_finalizer(js_State *J, void *node)
{
	map_collections.erase(node);
}

void
mjs_push_collection(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newarray(J);
	{
		js_newuserdata(J, "collection", node, mjs_htmlCollection_finalizer);
		addmethod(J, "item", mjs_htmlCollection_item, 1);
		addmethod(J, "namedItem", mjs_htmlCollection_namedItem, 1);
		addmethod(J, "toString", mjs_htmlCollection_toString, 0);
		addproperty(J, "length", mjs_htmlCollection_get_property_length, NULL);
		mjs_htmlCollection_set_items(J, node);
	}
	map_collections[node] = node;
}
