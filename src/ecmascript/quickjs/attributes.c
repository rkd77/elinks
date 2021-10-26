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

static JSClassID js_attributes_class_id;

static void
js_attributes_set_items(JSContext *ctx, JSValue this_val, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);

	int counter = 0;

	xmlpp::Element::AttributeList *al = JS_GetOpaque(this_val, js_attributes_class_id);

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

	xmlpp::Element::AttributeList *al = JS_GetOpaque(this_val, js_attributes_class_id);

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
	xmlpp::Element::AttributeList *al = JS_GetOpaque(this_val, js_attributes_class_id);

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
		return JS_EXCEPTION;
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
	xmlpp::Element::AttributeList *al = JS_GetOpaque(this_val, js_attributes_class_id);

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
			return obj;
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

	return ret;
}

static const JSCFunctionListEntry js_attributes_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_attributes_get_property_length, nullptr),
	JS_CFUNC_DEF("item", 1, js_attributes_item),
	JS_CFUNC_DEF("getNamedItem", 1, js_attributes_getNamedItem),
};

static JSClassDef js_attributes_class = {
	"attributes",
};

static JSValue
js_attributes_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_attributes_class_id);
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
js_attributes_init(JSContext *ctx, JSValue global_obj)
{
	JSValue attributes_proto, attributes_class;

	/* create the attributes class */
	JS_NewClassID(&js_attributes_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_attributes_class_id, &js_attributes_class);

	attributes_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, attributes_proto, js_attributes_proto_funcs, countof(js_attributes_proto_funcs));

	attributes_class = JS_NewCFunction2(ctx, js_attributes_ctor, "attributes", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, attributes_class, attributes_proto);
	JS_SetClassProto(ctx, js_attributes_class_id, attributes_proto);

	JS_SetPropertyStr(ctx, global_obj, "attributes", attributes_proto);
	return 0;
}



JSValue
getAttributes(JSContext *ctx, void *node)
{
	JSValue attributes_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, attributes_obj, js_attributes_proto_funcs, countof(js_attributes_proto_funcs));
//	attributes_class = JS_NewCFunction2(ctx, js_attributes_ctor, "attributes", 0, JS_CFUNC_constructor, 0);
//	JS_SetConstructor(ctx, attributes_class, attributes_obj);
	JS_SetClassProto(ctx, js_attributes_class_id, attributes_obj);

	JS_SetOpaque(attributes_obj, node);
	js_attributes_set_items(ctx, attributes_obj, node);

	return attributes_obj;
}
