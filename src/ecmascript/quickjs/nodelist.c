/* The QuickJS nodeList object implementation. */

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
#include "ecmascript/quickjs.h"
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
#include <string>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static std::map<void *, JSValueConst> map_nodelist;
static std::map<JSValueConst, void *> map_rev_nodelist;

static void *
js_nodeList_GetOpaque(JSValueConst this_val)
{
	return map_rev_nodelist[this_val];
}

static void
js_nodeList_SetOpaque(JSValueConst this_val, void *node)
{
	if (!node) {
		map_rev_nodelist.erase(this_val);
	} else {
		map_rev_nodelist[this_val] = node;
	}
}

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

static JSValue
js_nodeList_item2(JSContext *ctx, JSValueConst this_val, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeList *nl = static_cast<xmlpp::Node::NodeList *>(js_nodeList_GetOpaque(this_val));

	if (!nl) {
		return JS_UNDEFINED;
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
		return JS_UNDEFINED;
	}

	return getElement(ctx, element);
}

static JSValue
js_nodeList_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_UNDEFINED;
	}

	int index;
	JS_ToInt32(ctx, &index, argv[0]);

	return js_nodeList_item2(ctx, this_val, index);
}

static void
js_nodeList_set_items(JSContext *ctx, JSValue this_val, void *node)
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
			JSValue obj = getElement(ctx, element);

			JS_SetPropertyUint32(ctx, this_val, i, obj);
		}
	}
}

static JSValue
js_nodeList_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return JS_NewString(ctx, "[nodeList object]");
}

static const JSCFunctionListEntry js_nodeList_proto_funcs[] = {
//	JS_CGETSET_DEF("length", js_nodeList_get_property_length, nullptr),
	JS_CFUNC_DEF("item", 1, js_nodeList_item),
	JS_CFUNC_DEF("toString", 0, js_nodeList_toString)
};

#if 0
static void
js_nodeList_finalizer(JSRuntime *rt, JSValue val)
{
	void *node = js_nodeList_GetOpaque(val);

	js_nodeList_SetOpaque(val, nullptr);
	map_nodelist.erase(node);
}

static JSClassDef js_nodeList_class = {
	"nodeList",
	js_nodeList_finalizer
};
#endif

JSValue
getNodeList(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	auto node_find = map_nodelist.find(node);

	if (node_find != map_nodelist.end()) {
		JSValue r = JS_DupValue(ctx, node_find->second);
		RETURN_JS(r);
	}

	JSValue nodeList_obj = JS_NewArray(ctx);
	JS_SetPropertyFunctionList(ctx, nodeList_obj, js_nodeList_proto_funcs, countof(js_nodeList_proto_funcs));

	map_nodelist[node] = nodeList_obj;
	js_nodeList_SetOpaque(nodeList_obj, node);
	js_nodeList_set_items(ctx, nodeList_obj, node);

	RETURN_JS(nodeList_obj);
}
