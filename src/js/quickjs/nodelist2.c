/* The Quickjs nodeList2 object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/libdom/dom.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/ecmascript-c.h"
#include "js/quickjs/mapa.h"
#include "js/quickjs.h"
#include "js/quickjs/element.h"
#include "js/quickjs/node.h"
#include "js/quickjs/nodelist2.h"
#include "js/quickjs/window.h"
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

#if 0
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
#endif

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
	uint32_t index;
	JS_ToUint32(ctx, &index, argv[0]);

	return JS_GetPropertyUint32(ctx, this_val, index);
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

	LIST_OF(struct selector_node) *sni = (LIST_OF(struct selector_node) *)nodes;

	struct selector_node *sn = NULL;
	int i = 0;

	foreach (sn, *sni) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_ref((dom_node *)(sn->node));
		JSValue obj = getNode(ctx, sn->node);
		REF_JS(obj);

		JS_SetPropertyUint32(ctx, proto, i, JS_DupValue(ctx, obj));
		JS_FreeValue(ctx, obj);
		i++;
	}
	JSValue rr = JS_DupValue(ctx, proto);
	RETURN_JS(rr);
}
