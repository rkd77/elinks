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

#include "js/ecmascript.h"
#include "js/quickjs/mapa.h"
#include "js/quickjs.h"
#include "js/quickjs/element.h"
#include "js/quickjs/node.h"
#include "js/quickjs/nodelist.h"


#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_nodelist_class_id;

struct js_nodelist {
	JSValue arr;
	void *node;
};

void *map_nodelist;
void *map_rev_nodelist;

//static std::map<void *, JSValueConst> map_nodelist;
//static std::map<JSValueConst, void *> map_rev_nodelist;

static void *
js_nodelist_GetOpaque(JSValueConst this_val)
{
	ELOG
	REF_JS(this_val);

	return JS_GetOpaque(this_val, js_nodelist_class_id);
}

static void
js_nodelist_finalizer(JSRuntime *rt, JSValue val)
{
	ELOG
	REF_JS(val);
	struct js_nodelist *nodelist_private = (struct js_nodelist *)js_nodelist_GetOpaque(val);

	if (!nodelist_private) {
		return;
	}
	dom_nodelist *nl = (dom_nodelist *)(nodelist_private->node);
	dom_nodelist_unref(nl);
	mem_free(nodelist_private);
}

static void
js_nodelist_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct js_nodelist *nodelist_private = (struct js_nodelist *)js_nodelist_GetOpaque(val);

	if (!nodelist_private) {
		return;
	}
	JS_MarkValue(rt, nodelist_private->arr, mark_func);
}

static JSValue
js_nodeList_get_property_length(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct js_nodelist *nodelist_private = (struct js_nodelist *)js_nodelist_GetOpaque(this_val);

	if (!nodelist_private) {
		return JS_NULL;
	}
	dom_nodelist *nl = (dom_nodelist *)(nodelist_private->node);
	dom_exception err;
	uint32_t size;

	if (!nl) {
		return JS_NewInt32(ctx, 0);
	}
	dom_nodelist_ref(nl);
	err = dom_nodelist_get_length(nl, &size);

	if (err != DOM_NO_ERR) {
		dom_nodelist_unref(nl);
		return JS_NewInt32(ctx, 0);
	}
	dom_nodelist_unref(nl);

	return JS_NewInt32(ctx, size);
}

static JSValue
js_nodeList_item2(JSContext *ctx, JSValueConst this_val, int idx)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct js_nodelist *nodelist_private = (struct js_nodelist *)js_nodelist_GetOpaque(this_val);

	if (!nodelist_private) {
		return JS_NULL;
	}
	dom_nodelist *nl = (dom_nodelist *)(nodelist_private->node);
	dom_node *element = NULL;
	dom_exception err;
	JSValue ret;

	if (!nl) {
		return JS_UNDEFINED;
	}
	dom_nodelist_ref(nl);
	err = dom_nodelist_item(nl, idx, (void *)&element);

	if (err != DOM_NO_ERR || !element) {
		dom_nodelist_unref(nl);
		return JS_UNDEFINED;
	}
	ret = getNode(ctx, element);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(element);
	dom_nodelist_unref(nl);

	return ret;
}

static JSValue
js_nodeList_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_UNDEFINED;
	}
	int32_t index;
	JS_ToInt32(ctx, &index, argv[0]);

	return js_nodeList_item2(ctx, this_val, index);
}

static void
js_nodeList_set_items(JSContext *ctx, JSValue this_val, void *node)
{
	ELOG
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
	//dom_nodelist_ref(nl);
	err = dom_nodelist_get_length(nl, &length);

	if (err != DOM_NO_ERR) {
		//dom_nodelist_unref(nl);
		return;
	}

	for (i = 0; i < length; i++) {
		dom_node *element = NULL;
		err = dom_nodelist_item(nl, i, &element);
		JSValue obj;

		if (err != DOM_NO_ERR || !element) {
			continue;
		}

		obj = getNode(ctx, element);
		REF_JS(obj);

		JS_SetPropertyUint32(ctx, this_val, i, JS_DupValue(ctx, obj));
		JS_FreeValue(ctx, obj);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(element);
	}
	//dom_nodelist_unref(nl);
}

static JSValue
js_nodeList_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[nodeList object]");
}

static const JSCFunctionListEntry js_nodelist_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_nodeList_get_property_length, NULL),
	JS_CFUNC_DEF("item", 1, js_nodeList_item),
	JS_CFUNC_DEF("toString", 0, js_nodeList_toString)
};

static int
js_obj_delete_property(JSContext *ctx, JSValueConst obj, JSAtom prop)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_nodelist *nodelist_private = (struct js_nodelist *)js_nodelist_GetOpaque(obj);

	if (!nodelist_private) {
		return -1;
	}
	return JS_DeleteProperty(ctx, nodelist_private->arr, prop, 0);
}

static JSValue
js_obj_get_property(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst receiver)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_nodelist *nodelist_private = (struct js_nodelist *)js_nodelist_GetOpaque(obj);

	if (!nodelist_private) {
		return JS_NULL;
	}
	return JS_GetProperty(ctx, nodelist_private->arr, prop);
}

/* return < 0 if exception or TRUE/FALSE */
static int
js_obj_set_property(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst val, JSValueConst receiver, int flags)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct js_nodelist *nodelist_private = (struct js_nodelist *)js_nodelist_GetOpaque(obj);

	if (!nodelist_private) {
		return -1;
	}
	return JS_SetProperty(ctx, nodelist_private->arr, prop, val);
}


static JSClassExoticMethods exo = {
	.delete_property = js_obj_delete_property,
//	.has_property = js_obj_has_property,
	.get_property = js_obj_get_property,
	.set_property = js_obj_set_property
};

static JSClassDef js_nodelist_class = {
	"nodelist",
	.finalizer = js_nodelist_finalizer,
	.gc_mark = js_nodelist_mark,
	.exotic = &exo
};

JSValue
getNodeList(JSContext *ctx, void *node)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto;

	/* nodelist class */
	static int initialized;

	if (!initialized) {
		JS_NewClassID(&js_nodelist_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_nodelist_class_id, &js_nodelist_class);
		initialized = 1;
	}
	struct js_nodelist *nodelist_private = calloc(1, sizeof(*nodelist_private));

	if (!nodelist_private) {
		return JS_NULL;
	}
	proto = JS_NewArray(ctx);
	js_nodeList_set_items(ctx, proto, node);
	REF_JS(proto);

	JSValue nodelist_obj = JS_NewObjectClass(ctx, js_nodelist_class_id);
	REF_JS(nodelist_obj);

	JS_SetPropertyFunctionList(ctx, nodelist_obj, js_nodelist_proto_funcs, countof(js_nodelist_proto_funcs));
	JS_SetClassProto(ctx, js_nodelist_class_id, nodelist_obj);

	nodelist_private->arr = JS_DupValue(ctx, proto);
	JS_FreeValue(ctx, proto);
	nodelist_private->node = node;
	JS_SetOpaque(nodelist_obj, nodelist_private);
	JSValue rr = JS_DupValue(ctx, nodelist_obj);

	RETURN_JS(rr);
}
