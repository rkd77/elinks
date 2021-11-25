/* The QuickJS attr object implementation. */

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

static JSClassID js_attr_class_id;

static JSValue
js_attr_get_property_name(JSContext *ctx, JSValueConst this_val)
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

	xmlpp::AttributeNode *attr = JS_GetOpaque(this_val, js_attr_class_id);

	if (!attr) {
		return JS_NULL;
	}

	xmlpp::ustring v = attr->get_name();

	JSValue r = JS_NewString(ctx, v.c_str());

	RETURN_JS(r);
}

static JSValue
js_attr_get_property_value(JSContext *ctx, JSValueConst this_val)
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

	xmlpp::AttributeNode *attr = JS_GetOpaque(this_val, js_attr_class_id);

	if (!attr) {
		return JS_NULL;
	}

	xmlpp::ustring v = attr->get_value();

	JSValue r = JS_NewString(ctx, v.c_str());

	RETURN_JS(r);
}

static const JSCFunctionListEntry js_attr_proto_funcs[] = {
	JS_CGETSET_DEF("name", js_attr_get_property_name, nullptr),
	JS_CGETSET_DEF("value", js_attr_get_property_value, nullptr),
};

static std::map<void *, JSValueConst> map_attrs;

static
void js_attr_finalizer(JSRuntime *rt, JSValue val)
{
	void *node = JS_GetOpaque(val, js_attr_class_id);

	map_attrs.erase(node);
}

static JSClassDef js_attr_class = {
	"attr",
	js_attr_finalizer
};

JSValue
getAttr(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	static int initialized;
	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_attr_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_attr_class_id, &js_attr_class);
		initialized = 1;
	}

	auto node_find = map_attrs.find(node);

	if (node_find != map_attrs.end()) {
		JSValue r = JS_DupValue(ctx, node_find->second);
		RETURN_JS(r);
	}

	JSValue attr_obj = JS_NewObjectClass(ctx, js_attr_class_id);

	JS_SetPropertyFunctionList(ctx, attr_obj, js_attr_proto_funcs, countof(js_attr_proto_funcs));
	JS_SetClassProto(ctx, js_attr_class_id, attr_obj);
	JS_SetOpaque(attr_obj, node);

	map_attrs[node] = attr_obj;

	JSValue rr = JS_DupValue(ctx, attr_obj);
	RETURN_JS(rr);
}
