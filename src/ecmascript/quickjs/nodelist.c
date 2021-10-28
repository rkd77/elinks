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

static JSClassID js_nodeList_class_id;

static JSValue
js_nodeList_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeList *nl = JS_GetOpaque(this_val, js_nodeList_class_id);

	if (!nl) {
		return JS_NewInt32(ctx, 0);
	}

	return JS_NewInt32(ctx, nl->size());
}

static JSValue
js_nodeList_item2(JSContext *ctx, JSValueConst this_val, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Node::NodeList *nl = JS_GetOpaque(this_val, js_nodeList_class_id);

	if (!nl) {
		return JS_UNDEFINED;
	}

	xmlpp::Element *element = nullptr;

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

	xmlpp::Node::NodeList *nl = JS_GetOpaque(this_val, js_nodeList_class_id);

	if (!nl) {
		return;
	}

	auto it = nl->begin();
	auto end = nl->end();
	for (int i = 0; it != end; ++it, ++i) {
		xmlpp::Element *element = *it;

		if (element) {
			JSValue obj = getElement(ctx, element);

			JS_SetPropertyUint32(ctx, this_val, i, obj);
		}
	}
}

static const JSCFunctionListEntry js_nodeList_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_nodeList_get_property_length, nullptr),
	JS_CFUNC_DEF("item", 1, js_nodeList_item),
};

static JSClassDef js_nodeList_class = {
	"nodeList",
};

static JSValue
js_nodeList_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_nodeList_class_id);
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
js_nodeList_init(JSContext *ctx, JSValue global_obj)
{
	JSValue nodeList_proto, nodeList_class;

	/* create the nodeList class */
	JS_NewClassID(&js_nodeList_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_nodeList_class_id, &js_nodeList_class);

	nodeList_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, nodeList_proto, js_nodeList_proto_funcs, countof(js_nodeList_proto_funcs));

	nodeList_class = JS_NewCFunction2(ctx, js_nodeList_ctor, "nodeList", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, nodeList_class, nodeList_proto);
	JS_SetClassProto(ctx, js_nodeList_class_id, nodeList_proto);

	JS_SetPropertyStr(ctx, global_obj, "nodeList", nodeList_proto);
	return 0;
}

JSValue
getNodeList(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue nodeList_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, nodeList_obj, js_nodeList_proto_funcs, countof(js_nodeList_proto_funcs));
//	nodeList_class = JS_NewCFunction2(ctx, js_nodeList_ctor, "nodeList", 0, JS_CFUNC_constructor, 0);
//	JS_SetConstructor(ctx, nodeList_class, nodeList_obj);
	JS_SetClassProto(ctx, js_nodeList_class_id, nodeList_obj);

	JS_SetOpaque(nodeList_obj, node);
	js_nodeList_set_items(ctx, nodeList_obj, node);

	return nodeList_obj;
}

