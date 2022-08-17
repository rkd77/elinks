/* The MuJS nodeList object implementation. */

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
#include "ecmascript/mujs/element.h"
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
#include <string>

static std::map<void *, void *> map_nodelist;
static std::map<void *, void *> map_rev_nodelist;


#if 0
static JSValue
js_nodeList_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeList *nl = static_cast<xmlpp::Node::NodeList *>(js_nodeList_GetOpaque(this_val));

	if (!nl) {
		return JS_NewInt32(ctx, 0);
	}

	return JS_NewInt32(ctx, nl->size());
}
#endif

static void
mjs_push_nodeList_item2(js_State *J, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeList *nl = static_cast<xmlpp::Node::NodeList *>(js_touserdata(J, 0, "nodelist"));

	if (!nl) {
		js_pushundefined(J);
		return;
	}

	xmlpp::Node *element = nullptr;

	auto it = nl->begin();
	auto end = nl->end();
	for (int i = 0; it != end; ++it, ++i) {
		if (i == idx) {
			element = *it;
			break;
		}
	}

	if (!element) {
		js_pushundefined(J);
		return;
	}
	mjs_push_element(J, element);
}

static void
mjs_nodeList_item(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int index = js_toint32(J, 1);

	mjs_push_nodeList_item2(J, index);
}

static void
mjs_nodeList_set_items(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	xmlpp::Node::NodeList *nl = static_cast<xmlpp::Node::NodeList *>(node);

	if (!nl) {
		return;
	}

	auto it = nl->begin();
	auto end = nl->end();
	for (int i = 0; it != end; ++it, ++i) {
		xmlpp::Node *element = *it;

		if (element) {
			mjs_push_element(J, element);
			js_setindex(J, 1, i);
		}
	}
}

static void
mjs_nodeList_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[nodeList object]");
}

static void
mjs_nodeList_finalizer(js_State *J, void *node)
{
	map_nodelist.erase(node);
}

void
mjs_push_nodelist(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newobject(J);
	{
		js_newuserdata(J, "nodelist", node, mjs_nodeList_finalizer);
		addmethod(J, "item", mjs_nodeList_item, 1);
		addmethod(J, "toString", mjs_nodeList_toString, 0);
		mjs_nodeList_set_items(J, node);
	}
	map_nodelist[node] = node;
}
