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

	return JS_NewString(ctx, v.c_str());
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

	return JS_NewString(ctx, v.c_str());
}

static const JSCFunctionListEntry js_attr_proto_funcs[] = {
	JS_CGETSET_DEF("name", js_attr_get_property_name, nullptr),
	JS_CGETSET_DEF("value", js_attr_get_property_value, nullptr),
};

static JSClassDef js_attr_class = {
	"attr",
};

static JSValue
js_attr_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_attr_class_id);
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
js_attr_init(JSContext *ctx, JSValue global_obj)
{
	JSValue attr_proto, attr_class;

	/* create the attr class */
	JS_NewClassID(&js_attr_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_attr_class_id, &js_attr_class);

	attr_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, attr_proto, js_attr_proto_funcs, countof(js_attr_proto_funcs));

	attr_class = JS_NewCFunction2(ctx, js_attr_ctor, "attr", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, attr_class, attr_proto);
	JS_SetClassProto(ctx, js_attr_class_id, attr_proto);

	JS_SetPropertyStr(ctx, global_obj, "attr", attr_proto);
	return 0;
}

JSValue
getAttr(JSContext *ctx, void *node)
{
	JSValue attr_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, attr_obj, js_attr_proto_funcs, countof(js_attr_proto_funcs));
//	attr_class = JS_NewCFunction2(ctx, js_attr_ctor, "attr", 0, JS_CFUNC_constructor, 0);
//	JS_SetConstructor(ctx, attr_class, attr_obj);
	JS_SetClassProto(ctx, js_attr_class_id, attr_obj);

	JS_SetOpaque(attr_obj, node);

	return attr_obj;
}
