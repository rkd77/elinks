/* The QuickJS html collection object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "document/libdom/corestrings.h"
#include "js/ecmascript.h"
#include "js/ecmascript-c.h"
#include "js/quickjs/mapa.h"
#include "js/quickjs.h"
#include "js/quickjs/collection.h"
#include "js/quickjs/element.h"
#include "js/quickjs/node.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

JSClassID js_htmlCollection_class_id;

struct js_col {
	JSValue arr;
	void *node;
	unsigned int was_class_name:1;
};

static void *
js_htmlCollection_GetOpaque(JSValueConst this_val)
{
	ELOG
	REF_JS(this_val);

	return JS_GetOpaque(this_val, js_htmlCollection_class_id);
}

#if 0
static void
js_htmlCollection_SetOpaque(JSValueConst this_val, void *node)
{
	ELOG
	REF_JS(this_val);

	JS_SetOpaque(this_val, node);
#if 0
	if (!node) {
		attr_erase_from_map_rev(map_rev_collections, this_val);
	} else {
		attr_save_in_map_rev(map_rev_collections, this_val, node);
	}
#endif
}
#endif

static void
js_htmlColection_finalizer(JSRuntime *rt, JSValue val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);
	struct js_col *col_private = (struct js_col *)js_htmlCollection_GetOpaque(val);

	if (!col_private) {
		return;
	}
	if (col_private->was_class_name) {
		struct el_dom_html_collection *ns = (struct el_dom_html_collection *)(col_private->node);

		if (ns) {
			if (ns->refcnt > 0) {
				free_el_dom_collection(ns->ctx);
				ns->ctx = NULL;
				dom_html_collection_unref((dom_html_collection *)ns);
			}
		}
	} else {
		dom_html_collection *ns = (dom_html_collection *)(col_private->node);

		if (ns) {
			dom_html_collection_unref(ns);
		}
	}
	mem_free(col_private);
}

static void
js_htmlCollection_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct js_col *col_private = (struct js_col *)js_htmlCollection_GetOpaque(val);

	if (!col_private) {
		return;
	}
	JS_MarkValue(rt, col_private->arr, mark_func);
}

static JSValue
js_htmlCollection_get_property_length(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct js_col *col_private = (struct js_col *)js_htmlCollection_GetOpaque(this_val);

	if (!col_private) {
		return JS_NULL;
	}
	dom_html_collection *ns = (dom_html_collection *)(col_private->node);
	uint32_t size;

	if (!ns) {
		return JS_NewInt32(ctx, 0);
	}
	dom_html_collection_ref(ns);

	if (dom_html_collection_get_length(ns, &size) != DOM_NO_ERR) {
		dom_html_collection_unref(ns);
		return JS_NewInt32(ctx, 0);
	}
	dom_html_collection_unref(ns);

	return JS_NewInt32(ctx, size);
}

static JSValue
js_htmlCollection_item2(JSContext *ctx, JSValueConst this_val, int idx)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct js_col *col_private = (struct js_col *)js_htmlCollection_GetOpaque(this_val);

	if (!col_private) {
		return JS_NULL;
	}
	dom_html_collection *ns = (dom_html_collection *)(col_private->node);
	dom_node *node;
	dom_exception err;
	JSValue ret;

	if (!ns) {
		return JS_UNDEFINED;
	}
	dom_html_collection_ref(ns);
	err = dom_html_collection_item(ns, idx, &node);

	if (err != DOM_NO_ERR) {
		dom_html_collection_unref(ns);
		return JS_UNDEFINED;
	}
	ret = getNode(ctx, node);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
	dom_html_collection_unref(ns);

	return ret;
}

static JSValue
js_htmlCollection_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_UNDEFINED;
	}

	int index;
	JS_ToInt32(ctx, &index, argv[0]);

	return js_htmlCollection_item2(ctx, this_val, index);
}

static JSValue
js_htmlCollection_namedItem2(JSContext *ctx, JSValueConst this_val, const char *str)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct js_col *col_private = (struct js_col *)js_htmlCollection_GetOpaque(this_val);

	if (!col_private) {
		return JS_NULL;
	}
	dom_html_collection *ns = (dom_html_collection *)(col_private->node);
	dom_exception err;
	dom_string *name;
	uint32_t size, i;

	if (!ns) {
		return JS_UNDEFINED;
	}
	dom_html_collection_ref(ns);

	if (dom_html_collection_get_length(ns, &size) != DOM_NO_ERR) {
		dom_html_collection_unref(ns);
		return JS_UNDEFINED;
	}
	err = dom_string_create((const uint8_t*)str, strlen(str), &name);

	if (err != DOM_NO_ERR) {
		dom_html_collection_unref(ns);
		return JS_UNDEFINED;
	}

	for (i = 0; i < size; i++) {
		dom_node *element = NULL;
		dom_string *val = NULL;

		err = dom_html_collection_item(ns, i, &element);

		if (err != DOM_NO_ERR || !element) {
			continue;
		}
		err = dom_element_get_attribute(element, corestring_dom_id, &val);

		if (err == DOM_NO_ERR && val) {
			if (dom_string_caseless_isequal(name, val)) {
				JSValue ret = getNode(ctx, element);
				dom_string_unref(val);
				dom_string_unref(name);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
				dom_node_unref(element);
				dom_html_collection_unref(ns);

				return ret;
			}
			dom_string_unref(val);
		}
		err = dom_element_get_attribute(element, corestring_dom_name, &val);

		if (err == DOM_NO_ERR && val) {
			if (dom_string_caseless_isequal(name, val)) {
				JSValue ret = getNode(ctx, element);
				dom_string_unref(val);
				dom_string_unref(name);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
				dom_node_unref(element);
				dom_html_collection_unref(ns);

				return ret;
			}
			dom_string_unref(val);
		}
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(element);
	}
	dom_string_unref(name);
	dom_html_collection_unref(ns);

	return JS_UNDEFINED;
}

static JSValue
js_htmlCollection_namedItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_UNDEFINED;
	}

	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}

	JSValue ret = js_htmlCollection_namedItem2(ctx, this_val, str);
	JS_FreeCString(ctx, str);

	RETURN_JS(ret);
}

static void
js_htmlCollection_set_items(JSContext *ctx, JSValue this_val, void *node)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	int counter = 0;
	uint32_t size, i;
	dom_html_collection *ns = (dom_html_collection *)(node);
	dom_exception err;

	if (!ns) {
		return;
	}
	dom_html_collection_ref(ns);

	if (dom_html_collection_get_length(ns, &size) != DOM_NO_ERR) {
		dom_html_collection_unref(ns);
		return;
	}

	for (i = 0; i < size; i++) {
		dom_node *element = NULL;
		dom_string *name = NULL;
		err = dom_html_collection_item(ns, i, &element);

		if (err != DOM_NO_ERR || !element) {
			continue;
		}
		JSValue obj = getNode(ctx, element);

		REF_JS(obj);

		JS_SetPropertyUint32(ctx, this_val, i, JS_DupValue(ctx, obj));
		err = dom_element_get_attribute(element, corestring_dom_id, &name);

		if (err != DOM_NO_ERR || !name) {
			err = dom_element_get_attribute(element, corestring_dom_name, &name);
		}

		if (err == DOM_NO_ERR && name) {
			if (!dom_string_caseless_lwc_isequal(name, corestring_lwc_item) && !dom_string_caseless_lwc_isequal(name, corestring_lwc_nameditem)) {
				JS_DefinePropertyValueStr(ctx, this_val, dom_string_data(name), JS_DupValue(ctx, obj), 0);
			}
		}
		JS_FreeValue(ctx, obj);
		counter++;
		dom_string_unref(name);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(element);
	}
	dom_html_collection_unref(ns);
}

static JSValue
js_htmlCollection_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[htmlCollection object]");
}

static const JSCFunctionListEntry js_htmlCollection_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_htmlCollection_get_property_length, NULL),
	JS_CFUNC_DEF("item", 1, js_htmlCollection_item),
	JS_CFUNC_DEF("namedItem", 1, js_htmlCollection_namedItem),
	JS_CFUNC_DEF("toString", 0, js_htmlCollection_toString)
};

static int
js_obj_delete_property(JSContext *ctx, JSValueConst obj, JSAtom prop)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_col *col_private = (struct js_col *)js_htmlCollection_GetOpaque(obj);

	if (!col_private) {
		return -1;
	}
	return JS_DeleteProperty(ctx, col_private->arr, prop, 0);
}

static JSValue
js_obj_get_property(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst receiver)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_col *col_private = (struct js_col *)js_htmlCollection_GetOpaque(obj);

	if (!col_private) {
		return JS_NULL;
	}
	return JS_GetProperty(ctx, col_private->arr, prop);
}

/* return < 0 if exception or TRUE/FALSE */
static int
js_obj_set_property(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst val, JSValueConst receiver, int flags)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_col *col_private = (struct js_col *)js_htmlCollection_GetOpaque(obj);

	if (!col_private) {
		return -1;
	}
	return JS_SetProperty(ctx, col_private->arr, prop, val);
}

static JSClassExoticMethods exo = {
	.delete_property = js_obj_delete_property,
//	.has_property = js_obj_has_property,
	.get_property = js_obj_get_property,
	.set_property = js_obj_set_property
};

static JSClassDef js_htmlCollection_class = {
	"htmlCollection",
	.finalizer = js_htmlColection_finalizer,
	.gc_mark = js_htmlCollection_mark,
	.exotic = &exo
};

static JSValue
getCollection_common(JSContext *ctx, void *node, bool was_class_name)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	static int initialized;

	if (!initialized) {
		/* collection class */
		JS_NewClassID(&js_htmlCollection_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_htmlCollection_class_id, &js_htmlCollection_class);
		initialized = 1;
	}
	struct js_col *col_private = calloc(1, sizeof(*col_private));

	if (!col_private) {
		return JS_NULL;
	}
	col_private->was_class_name = was_class_name;
	JSValue proto = JS_NewArray(ctx);
	js_htmlCollection_set_items(ctx, proto, node);

	JSValue col_obj = JS_NewObjectClass(ctx, js_htmlCollection_class_id);
	REF_JS(col_obj);

	JS_SetPropertyFunctionList(ctx, col_obj, js_htmlCollection_proto_funcs, countof(js_htmlCollection_proto_funcs));
	JS_SetClassProto(ctx, js_htmlCollection_class_id, col_obj);

	col_private->arr = JS_DupValue(ctx, proto);
	JS_FreeValue(ctx, proto);
	col_private->node = node;
	JS_SetOpaque(col_obj, col_private);
	JSValue rr = JS_DupValue(ctx, col_obj);

	RETURN_JS(rr);
}

JSValue
getCollection(JSContext *ctx, void *node)
{
	ELOG
	return getCollection_common(ctx, node, false);
}

JSValue
getCollection2(JSContext *ctx, void *node)
{
	ELOG
	return getCollection_common(ctx, node, true);
}
