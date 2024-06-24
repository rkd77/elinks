/* The Quickjs nodeList2 object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/libdom/dom.h"

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
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/element.h"
#include "ecmascript/quickjs/nodelist2.h"
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
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_nodelist2_class_id;

void *map_nodelist2;
void *map_rev_nodelist2;

static void *
js_nodeList2_GetOpaque(JSValueConst this_val)
{
	REF_JS(this_val);

	return attr_find_in_map_rev(map_rev_nodelist2, this_val);
}

static void
js_nodeList2_SetOpaque(JSValueConst this_val, void *node)
{
	REF_JS(this_val);

	if (!node) {
		attr_erase_from_map_rev(map_rev_nodelist2, this_val);
	} else {
		attr_save_in_map_rev(map_rev_nodelist2, this_val, node);
	}
}

static JSValue
js_nodeList2_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	LIST_OF(struct selector_node) *sni = (LIST_OF(struct selector_node) *)(js_nodeList2_GetOpaque(this_val));
	uint32_t size = list_size(sni);

	return JS_NewInt32(ctx, size);
}

static JSValue
js_nodeList2_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
	LIST_OF(struct selector_node) *sni = (LIST_OF(struct selector_node) *)(js_nodeList2_GetOpaque(this_val));
	int counter = 0;
	struct selector_node *sn = NULL;

	foreach (sn, *sni) {
		if (counter == index) {
			break;
		}
		counter++;
	}

	if (!sn || !sn->node) {
		return JS_NULL;
	}
	JSValue ret = getElement(ctx, sn->node);

	return ret;
}

static JSValue
js_nodeList2_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[nodeList2 object]");
}

static const JSCFunctionListEntry js_nodeList2_proto_funcs[] = {
//	JS_CGETSET_DEF("length", js_nodeList_get_property_length, NULL),
	JS_CFUNC_DEF("item", 1, js_nodeList2_item),
	JS_CFUNC_DEF("toString", 0, js_nodeList2_toString)
};

static JSClassDef js_nodelist2_class = {
	"nodelist2",
};

JSValue
getNodeList2(JSContext *ctx, void *nodes)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto;

	/* nodelist class */
	JS_NewClassID(&js_nodelist2_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_nodelist2_class_id, &js_nodelist2_class);
	proto = JS_NewArray(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_nodeList2_proto_funcs, countof(js_nodeList2_proto_funcs));
	JS_SetClassProto(ctx, js_nodelist2_class_id, proto);

	attr_save_in_map(map_nodelist2, nodes, proto);
	js_nodeList2_SetOpaque(proto, nodes);

	LIST_OF(struct selector_node) *sni = (LIST_OF(struct selector_node) *)nodes;

	struct selector_node *sn = NULL;
	int i = 0;

	foreach (sn, *sni) {
		JSValue obj = getElement(ctx, sn->node);
		REF_JS(obj);

		JS_SetPropertyUint32(ctx, proto, i, JS_DupValue(ctx, obj));
		JS_FreeValue(ctx, obj);
		i++;
	}
	JSValue rr = JS_DupValue(ctx, proto);
	RETURN_JS(rr);
}
