/* The QuickJS attr object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs/mapa.h"
#include "ecmascript/quickjs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_attr_class_id;

static JSValue
js_attr_get_property_name(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	dom_string *name = NULL;
	dom_exception err;
	JSValue r;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	dom_attr *attr = (dom_attr *)(JS_GetOpaque(this_val, js_attr_class_id));

	if (!attr) {
		return JS_NULL;
	}
	dom_node_ref(attr);
	err = dom_attr_get_name(attr, &name);

	if (err != DOM_NO_ERR || name == NULL) {
		dom_node_unref(attr);
		return JS_NULL;
	}

	r = JS_NewStringLen(ctx, dom_string_data(name), dom_string_length(name));
	dom_string_unref(name);
	dom_node_unref(attr);

	RETURN_JS(r);
}

static JSValue
js_attr_get_property_value(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	dom_string *value = NULL;
	dom_exception err;
	JSValue r;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}
	dom_attr *attr = (dom_attr *)(JS_GetOpaque(this_val, js_attr_class_id));

	if (!attr) {
		return JS_NULL;
	}
	dom_node_ref(attr);
	err = dom_attr_get_value(attr, &value);

	if (err != DOM_NO_ERR || value == NULL) {
		dom_node_unref(attr);
		return JS_NULL;
	}
	r = JS_NewStringLen(ctx, dom_string_data(value), dom_string_length(value));
	dom_string_unref(value);
	dom_node_unref(attr);

	RETURN_JS(r);
}

static JSValue
js_attr_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	return JS_NewString(ctx, "[attr object]");
}

static const JSCFunctionListEntry js_attr_proto_funcs[] = {
	JS_CGETSET_DEF("name", js_attr_get_property_name, NULL),
	JS_CGETSET_DEF("value", js_attr_get_property_value, NULL),
	JS_CFUNC_DEF("toString", 0, js_attr_toString)
};

void *map_attrs;

static
void js_attr_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);
	dom_attr *node = (dom_attr *)JS_GetOpaque(val, js_attr_class_id);

	attr_erase_from_map(map_attrs, node);

	if (node) {
		dom_node_unref(node);
	}
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
	JSValue second = JS_NULL;
	static int initialized;
	/* create the element class */
	if (!initialized) {
		JS_NewClassID(&js_attr_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_attr_class_id, &js_attr_class);
		initialized = 1;
	}
	second = attr_find_in_map(map_attrs, node);

	if (!JS_IsNull(second)) {
		JSValue r = JS_DupValue(ctx, second);

		RETURN_JS(r);
	}

	JSValue attr_obj = JS_NewObjectClass(ctx, js_attr_class_id);

	JS_SetPropertyFunctionList(ctx, attr_obj, js_attr_proto_funcs, countof(js_attr_proto_funcs));
	JS_SetClassProto(ctx, js_attr_class_id, attr_obj);
	JS_SetOpaque(attr_obj, node);

	attr_save_in_map(map_attrs, node, attr_obj);

	JSValue rr = JS_DupValue(ctx, attr_obj);

	RETURN_JS(rr);
}
