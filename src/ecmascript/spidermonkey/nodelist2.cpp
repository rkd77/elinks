/* The SpiderMonkey nodeList2 object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/libdom/dom.h"

#include "ecmascript/spidermonkey/util.h"
#include <jsfriendapi.h>

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
#include "ecmascript/spidermonkey/element.h"
#include "ecmascript/spidermonkey/nodelist2.h"
#include "ecmascript/spidermonkey/window.h"
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

#include <iostream>
#include <algorithm>
#include <string>

static bool nodeList2_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool nodeList2_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);

static void nodeList2_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	LIST_OF(struct selector_node) *sni = JS::GetMaybePtrFromReservedSlot<LIST_OF(struct selector_node)>(obj, 0);

	if (sni) {
		free_list(*sni);
	}
}

JSClassOps nodeList2_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nodeList2_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass nodeList2_class = {
	"nodeList2",
	JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_HAS_CACHED_PROTO(JSProto_Array),
	&nodeList2_ops
};

static const spidermonkeyFunctionSpec nodeList2_funcs[] = {
	{ "item",		nodeList2_item,		1 },
	{ NULL }
};

static bool nodeList2_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

static JSPropertySpec nodeList2_props[] = {
	JS_PSG("length",	nodeList2_get_property_length, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
nodeList2_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &nodeList2_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	LIST_OF(struct selector_node) *sni = JS::GetMaybePtrFromReservedSlot<LIST_OF(struct selector_node)>(hobj, 0);
	args.rval().setInt32(list_size(sni));

	return true;
}

static bool
nodeList2_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);
	LIST_OF(struct selector_node) *sni = JS::GetMaybePtrFromReservedSlot<LIST_OF(struct selector_node)>(hobj, 0);
	int index = args[0].toInt32();
	int counter = 0;
	struct selector_node *sn = NULL;

	foreach (sn, *sni) {
		if (counter == index) {
			break;
		}
		counter++;
	}

	if (!sn || !sn->node) {
		args.rval().setNull();
		return true;
	}

	JSObject *res = getElement(ctx, sn->node);
	args.rval().setObject(*res);
	return true;
}

JSObject *
getNodeList2(JSContext *ctx, void *res)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	LIST_OF(struct selector_node) *sni = (LIST_OF(struct selector_node) *)res;
	JSObject *el = JS_NewObject(ctx, &nodeList2_class);

	if (!el) {
		return NULL;
	}
	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) nodeList2_props);
	spidermonkey_DefineFunctions(ctx, el, nodeList2_funcs);

	struct selector_node *sn = NULL;
	int i = 0;

	foreach (sn, *sni) {
		JSObject *obj = getElement(ctx, sn->node);

		if (obj) {
			JS::RootedObject v(ctx, obj);
			JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));
			JS_SetElement(ctx, r_el, i, ro);
		}
		i++;
	}
	JS::SetReservedSlot(el, 0, JS::PrivateValue(res));

	return el;
}
