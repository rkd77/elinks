/* The MuJS attributes object implementation. */

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
#include "ecmascript/mujs/attr.h"
#include "ecmascript/mujs/attributes.h"
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
#include <string>

static std::map<void *, void *> map_attributes;
static std::map<void *, void *> map_rev_attributes;

static void
mjs_attributes_set_items(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);

	xmlpp::Element::AttributeList *al = static_cast<xmlpp::Element::AttributeList *>(node);

	if (!al) {
		return;
	}

	auto it = al->begin();
	auto end = al->end();
	int i = 0;

	js_newarray(J);

	for (;it != end; ++it, ++i) {
		xmlpp::Attribute *attr = *it;

		if (!attr) {
			continue;
		}
// TODO Check it
		mjs_push_attr(J, attr);
		js_setindex(J, -2, i);

		xmlpp::ustring name = attr->get_name();

		if (name != "" && name != "item" && name != "namedItem") {
			mjs_push_attr(J, attr);
			js_setproperty(J, -2, name.c_str());
		}
	}
}

static void
mjs_attributes_get_property_length(js_State *J)
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
	xmlpp::Element::AttributeList *al = static_cast<xmlpp::Element::AttributeList *>(js_touserdata(J, 0, "attribute"));

	if (!al) {
		js_pushnumber(J, 0);
		return;
	}
	js_pushnumber(J, al->size());
}

static void
mjs_push_attributes_item2(js_State *J, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element::AttributeList *al = static_cast<xmlpp::Element::AttributeList *>(js_touserdata(J, 0, "attribute"));

	if (!al) {
		js_pushundefined(J);
		return;
	}

	auto it = al->begin();
	auto end = al->end();
	int i = 0;

	for (;it != end; it++, i++) {
		if (i != idx) {
			continue;
		}
		xmlpp::Attribute *attr = *it;
		mjs_push_attr(J, attr);
		return;
	}
	js_pushundefined(J);
}

static void
mjs_attributes_item(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int index = js_toint32(J, 1);

	mjs_push_attributes_item2(J, index);
}

static void
mjs_push_attributes_namedItem2(js_State *J, const char *str)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element::AttributeList *al = static_cast<xmlpp::Element::AttributeList *>(js_touserdata(J, 0, "attribute"));

	if (!al) {
		js_pushundefined(J);
		return;
	}

	xmlpp::ustring name = str;

	auto it = al->begin();
	auto end = al->end();

	for (; it != end; ++it) {
		auto attr = dynamic_cast<xmlpp::AttributeNode*>(*it);

		if (!attr) {
			continue;
		}

		if (name == attr->get_name()) {
			mjs_push_attr(J, attr);
			return;
		}
	}
	js_pushundefined(J);
}

static void
mjs_attributes_getNamedItem(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	mjs_push_attributes_namedItem2(J, str);
}

static void
mjs_attributes_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[attributes object]");
}

static void
mjs_attributes_finalizer(js_State *J, void *node)
{
	map_attributes.erase(node);
}

void
mjs_push_attributes(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newobject(J);
	{
		js_newuserdata(J, "attribute", node, mjs_attributes_finalizer);
		addmethod(J, "item", mjs_attributes_item, 1);
		addmethod(J, "getNamedItem", mjs_attributes_getNamedItem, 1);
		addmethod(J, "toString", mjs_attributes_toString, 0);
		addproperty(J, "length", mjs_attributes_get_property_length, NULL);

		mjs_attributes_set_items(J, node);
	}
	map_attributes[node] = node;
}
