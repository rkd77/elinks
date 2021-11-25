/* The QuickJS attributes object implementation. */

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
#include "ecmascript/quickjs/attr.h"
#include "ecmascript/quickjs/attributes.h"
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

static std::map<void *, JSValueConst> map_attributes;
static std::map<JSValueConst, void *> map_rev_attributes;

static void *
js_attributes_GetOpaque(JSValueConst this_val)
{
	return map_rev_attributes[this_val];
}

static void
js_attributes_SetOpaque(JSValueConst this_val, void *node)
{
	if (!node) {
		map_rev_attributes.erase(this_val);
	} else {
		map_rev_attributes[this_val] = node;
	}
}

static void
js_attributes_set_items(JSContext *ctx, JSValue this_val, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);

	int counter = 0;

	xmlpp::Element::AttributeList *al = node;

	if (!al) {
		return;
	}

	auto it = al->begin();
	auto end = al->end();
	int i = 0;

	for (;it != end; ++it, ++i) {
		xmlpp::Attribute *attr = *it;

		if (!attr) {
			continue;
		}

		JSValue obj = getAttr(ctx, attr);
		JS_SetPropertyUint32(ctx, this_val, i, obj);

		xmlpp::ustring name = attr->get_name();

		if (name != "" && name != "item" && name != "namedItem") {
			JS_DefinePropertyValueStr(ctx, this_val, name.c_str(), obj, 0);
		}
	}
}

static JSValue
js_attributes_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	xmlpp::Element::AttributeList *al = js_attributes_GetOpaque(this_val);

	if (!al) {
		return JS_NewInt32(ctx, 0);
	}

	return JS_NewInt32(ctx, al->size());
}

static JSValue
js_attributes_item2(JSContext *ctx, JSValueConst this_val, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element::AttributeList *al = js_attributes_GetOpaque(this_val);

	if (!al) {
		return JS_UNDEFINED;
	}

	auto it = al->begin();
	auto end = al->end();
	int i = 0;

	for (;it != end; it++, i++) {
		if (i != idx) {
			continue;
		}
		xmlpp::Attribute *attr = *it;

		return getAttr(ctx, attr);
	}

	return JS_UNDEFINED;
}

static JSValue
js_attributes_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_UNDEFINED;
	}

	int index;
	JS_ToInt32(ctx, &index, argv[0]);

	return js_attributes_item2(ctx, this_val, index);
}

static JSValue
js_attributes_namedItem2(JSContext *ctx, JSValueConst this_val, const char *str)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	xmlpp::Element::AttributeList *al = js_attributes_GetOpaque(this_val);

	if (!al) {
		return JS_UNDEFINED;
	}

	xmlpp::ustring name = str;

	auto it = al->begin();
	auto end = al->end();

	for (; it != end; ++it) {
		const auto attr = dynamic_cast<const xmlpp::AttributeNode*>(*it);

		if (!attr) {
			continue;
		}

		if (name == attr->get_name()) {
			JSValue obj = getAttr(ctx, attr);
			RETURN_JS(obj);
		}
	}

	return JS_UNDEFINED;
}

static JSValue
js_attributes_getNamedItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (argc != 1) {
		return JS_UNDEFINED;
	}

	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}

	JSValue ret = js_attributes_namedItem2(ctx, this_val, str);
	JS_FreeCString(ctx, str);

	RETURN_JS(ret);
}

static const JSCFunctionListEntry js_attributes_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_attributes_get_property_length, nullptr),
	JS_CFUNC_DEF("item", 1, js_attributes_item),
	JS_CFUNC_DEF("getNamedItem", 1, js_attributes_getNamedItem),
};

static void
js_attributes_finalizer(JSRuntime *rt, JSValue val)
{
	void *node = js_attributes_GetOpaque(val);

	js_attributes_SetOpaque(val, nullptr);
	map_attributes.erase(node);
}

static JSClassDef js_attributes_class = {
	"attributes",
	js_attributes_finalizer
};

JSValue
getAttributes(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	auto node_find = map_attributes.find(node);

	if (node_find != map_attributes.end()) {
		JSValue r = JS_DupValue(ctx, node_find->second);
		RETURN_JS(r);
	}
	JSValue attributes_obj = JS_NewArray(ctx);
	JS_SetPropertyFunctionList(ctx, attributes_obj, js_attributes_proto_funcs, countof(js_attributes_proto_funcs));

	js_attributes_SetOpaque(attributes_obj, node);
	js_attributes_set_items(ctx, attributes_obj, node);
	map_attributes[node] = attributes_obj;

	JSValue rr = JS_DupValue(ctx, attributes_obj);
	RETURN_JS(rr);
}
