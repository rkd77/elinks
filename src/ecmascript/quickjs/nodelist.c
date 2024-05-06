/* The QuickJS nodeList object implementation. */

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

#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/element.h"
#include "ecmascript/quickjs/nodelist.h"


#define countof(x) (sizeof(x) / sizeof((x)[0]))

void *map_nodelist;
void *map_rev_nodelist;

//static std::map<void *, JSValueConst> map_nodelist;
//static std::map<JSValueConst, void *> map_rev_nodelist;

static void *
js_nodeList_GetOpaque(JSValueConst this_val)
{
	REF_JS(this_val);

	return attr_find_in_map_rev(map_rev_nodelist, this_val);
}

static void
js_nodeList_SetOpaque(JSValueConst this_val, void *node)
{
	REF_JS(this_val);

	if (!node) {
		attr_erase_from_map_rev(map_rev_nodelist, this_val);
	} else {
		attr_save_in_map_rev(map_rev_nodelist, this_val, node);
	}
}

static JSValue
js_nodeList_item2(JSContext *ctx, JSValueConst this_val, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	dom_nodelist *nl = (dom_nodelist *)(js_nodeList_GetOpaque(this_val));
	dom_node *element = NULL;
	dom_exception err;
	JSValue ret;

	if (!nl) {
		return JS_UNDEFINED;
	}
	err = dom_nodelist_item(nl, idx, (void *)&element);

	if (err != DOM_NO_ERR || !element) {
		return JS_UNDEFINED;
	}
	ret = getElement(ctx, element);
	dom_node_unref(element);

	return ret;
}

static JSValue
js_nodeList_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

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
	REF_JS(this_val);

	dom_nodelist *nl = (dom_nodelist *)(node);
	dom_exception err;
	uint32_t length, i;

	if (!nl) {
		return;
	}
	err = dom_nodelist_get_length(nl, &length);

	if (err != DOM_NO_ERR) {
		return;
	}

	for (i = 0; i < length; i++) {
		dom_node *element = NULL;
		err = dom_nodelist_item(nl, i, &element);
		JSValue obj;

		if (err != DOM_NO_ERR || !element) {
			continue;
		}

		obj = getElement(ctx, element);
		REF_JS(obj);

		JS_SetPropertyUint32(ctx, this_val, i, JS_DupValue(ctx, obj));
		JS_FreeValue(ctx, obj);
		dom_node_unref(element);
	}
}

static JSValue
js_nodeList_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[nodeList object]");
}

static const JSCFunctionListEntry js_nodeList_proto_funcs[] = {
//	JS_CGETSET_DEF("length", js_nodeList_get_property_length, nullptr),
	JS_CFUNC_DEF("item", 1, js_nodeList_item),
	JS_CFUNC_DEF("toString", 0, js_nodeList_toString)
};

JSValue
getNodeList(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue nodeList_obj = JS_NewArray(ctx);
	JS_SetPropertyFunctionList(ctx, nodeList_obj, js_nodeList_proto_funcs, countof(js_nodeList_proto_funcs));

	attr_save_in_map(map_nodelist, node, nodeList_obj);
	js_nodeList_SetOpaque(nodeList_obj, node);
	js_nodeList_set_items(ctx, nodeList_obj, node);

	return nodeList_obj;

//	JSValue rr = JS_DupValue(ctx, nodeList_obj);

//	RETURN_JS(rr);
}
