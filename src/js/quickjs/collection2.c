/* The QuickJS html collection2 object implementation. */

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
#include "js/ecmascript-c.h"
#include "js/ecmascript.h"
#include "js/quickjs/mapa.h"
#include "js/quickjs.h"
#include "js/quickjs/collection.h"
#include "js/quickjs/element.h"
#include "js/quickjs/node.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

void *map_collections2;
void *map_rev_collections2;

JSClassID js_htmlCollection2_class_id;

static void *
js_htmlCollection2_GetOpaque(JSValueConst this_val)
{
	ELOG
	REF_JS(this_val);

	return JS_GetOpaque(this_val, js_htmlCollection2_class_id);
}

static void
js_htmlCollection2_SetOpaque(JSValueConst this_val, void *node)
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

static
void js_htmlColection2_finalizer(JSRuntime *rt, JSValue val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)(js_htmlCollection2_GetOpaque(val));

	if (ns) {
		//attr_erase_from_map_str(map_collections, ns);
		if (ns->refcnt > 0) {
			if (ns->ctx) {
				free_el_dom_collection(ns->ctx);
				ns->ctx = NULL;
			}
			dom_html_collection_unref((dom_html_collection *)ns);
		}
	}
}

static JSValue
js_htmlCollection2_get_property_length(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)(js_htmlCollection2_GetOpaque(this_val));
	uint32_t size;

	if (!ns) {
		return JS_NewInt32(ctx, 0);
	}
	dom_html_collection_ref((dom_html_collection *)ns);

	if (dom_html_collection_get_length((dom_html_collection *)ns, &size) != DOM_NO_ERR) {
		dom_html_collection_unref((dom_html_collection *)ns);
		return JS_NewInt32(ctx, 0);
	}
	dom_html_collection_unref((dom_html_collection *)ns);

	return JS_NewInt32(ctx, size);
}

static JSValue
js_htmlCollection2_item2(JSContext *ctx, JSValueConst this_val, int idx)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)(js_htmlCollection2_GetOpaque(this_val));
	dom_node *node;
	dom_exception err;
	JSValue ret;

	if (!ns) {
		return JS_UNDEFINED;
	}
	dom_html_collection_ref((dom_html_collection *)ns);
	err = dom_html_collection_item((dom_html_collection *)ns, idx, &node);

	if (err != DOM_NO_ERR) {
		dom_html_collection_unref((dom_html_collection *)ns);
		return JS_UNDEFINED;
	}
	ret = getNode(ctx, node);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
	dom_html_collection_unref((dom_html_collection *)ns);

	return ret;
}

static JSValue
js_htmlCollection2_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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

	return js_htmlCollection2_item2(ctx, this_val, index);
}

static JSValue
js_htmlCollection2_namedItem2(JSContext *ctx, JSValueConst this_val, const char *str)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)(js_htmlCollection2_GetOpaque(this_val));
	dom_exception err;
	dom_string *name;
	uint32_t size, i;

	if (!ns) {
		return JS_UNDEFINED;
	}
	dom_html_collection_ref((dom_html_collection *)ns);

	if (dom_html_collection_get_length((dom_html_collection *)ns, &size) != DOM_NO_ERR) {
		dom_html_collection_unref((dom_html_collection *)ns);
		return JS_UNDEFINED;
	}
	err = dom_string_create((const uint8_t*)str, strlen(str), &name);

	if (err != DOM_NO_ERR) {
		dom_html_collection_unref((dom_html_collection *)ns);
		return JS_UNDEFINED;
	}

	for (i = 0; i < size; i++) {
		dom_node *element = NULL;
		dom_string *val = NULL;

		err = dom_html_collection_item((dom_html_collection *)ns, i, &element);

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
				dom_html_collection_unref((dom_html_collection *)ns);

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
				dom_html_collection_unref((dom_html_collection *)ns);

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
	dom_html_collection_unref((dom_html_collection *)ns);

	return JS_UNDEFINED;
}

static JSValue
js_htmlCollection2_namedItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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

	JSValue ret = js_htmlCollection2_namedItem2(ctx, this_val, str);
	JS_FreeCString(ctx, str);

	RETURN_JS(ret);
}

static void
js_htmlCollection2_set_items(JSContext *ctx, JSValue this_val, void *node)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	int counter = 0;
	uint32_t size, i;
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)(js_htmlCollection2_GetOpaque(this_val));
	dom_exception err;

	if (!ns) {
		return;
	}
	dom_html_collection_ref((dom_html_collection *)ns);

	if (dom_html_collection_get_length((dom_html_collection *)ns, &size) != DOM_NO_ERR) {
		dom_html_collection_unref((dom_html_collection *)ns);
		return;
	}

	for (i = 0; i < size; i++) {
		dom_node *element = NULL;
		dom_string *name = NULL;
		err = dom_html_collection_item((dom_html_collection *)ns, i, &element);

		if (err != DOM_NO_ERR || !element) {
			continue;
		}
		JSValue obj = getNode(ctx, element);

		REF_JS(obj);
		JS_SetPropertyUint32(ctx, this_val, counter, JS_DupValue(ctx, obj));
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
	dom_html_collection_unref((dom_html_collection *)ns);
}

static JSValue
js_htmlCollection2_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[htmlCollection2 object]");
}

static const JSCFunctionListEntry js_htmlCollection2_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_htmlCollection2_get_property_length, NULL),
	JS_CFUNC_DEF("item", 1, js_htmlCollection2_item),
	JS_CFUNC_DEF("namedItem", 1, js_htmlCollection2_namedItem),
	JS_CFUNC_DEF("toString", 0, js_htmlCollection2_toString)
};

static JSClassDef js_htmlCollection2_class = {
	"htmlCollection2",
	.finalizer = js_htmlColection2_finalizer
};

JSValue
getCollection2(JSContext *ctx, void *node)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	static int initialized;

	if (!initialized) {
		/* collection class */
		JS_NewClassID(&js_htmlCollection2_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_htmlCollection2_class_id, &js_htmlCollection2_class);
		initialized = 1;
	}
	JSValue proto = JS_NewObjectClass(ctx, js_htmlCollection2_class_id);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_htmlCollection2_proto_funcs, countof(js_htmlCollection2_proto_funcs));
	JS_SetClassProto(ctx, js_htmlCollection2_class_id, proto);

	js_htmlCollection2_SetOpaque(proto, node);
//	js_htmlCollection2_set_items(ctx, proto, node);
//	attr_save_in_map(map_collections, node, proto);
	JSValue rr = JS_DupValue(ctx, proto);

	RETURN_JS(rr);
}
