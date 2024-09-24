/* The SpiderMonkey nodeList object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/libdom/dom.h"

#include "js/spidermonkey/util.h"
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
#include "js/ecmascript.h"
#include "js/spidermonkey/element.h"
#include "js/spidermonkey/nodelist.h"
#include "js/spidermonkey/window.h"
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

#include <iostream>
#include <algorithm>
#include <string>

static bool nodeList_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool nodeList_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);

static void nodeList_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_nodelist *nl = JS::GetMaybePtrFromReservedSlot<dom_nodelist>(obj, 0);

	if (nl) {
		dom_nodelist_unref(nl);
	}
}

JSClassOps nodeList_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nodeList_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass nodeList_class = {
	"nodeList",
	JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_HAS_CACHED_PROTO(JSProto_Array),
	&nodeList_ops
};

static const spidermonkeyFunctionSpec nodeList_funcs[] = {
	{ "item",		nodeList_item,		1 },
	{ NULL }
};

static bool nodeList_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

static JSPropertySpec nodeList_props[] = {
	JS_PSG("length",	nodeList_get_property_length, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
nodeList_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &nodeList_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_nodelist *nl = JS::GetMaybePtrFromReservedSlot<dom_nodelist>(hobj, 0);
	dom_exception err;
	uint32_t size;

	if (!nl) {
		args.rval().setInt32(0);
		return true;
	}
	err = dom_nodelist_get_length(nl, &size);

	if (err != DOM_NO_ERR) {
		args.rval().setInt32(0);
		return true;
	}
	args.rval().setInt32(size);

	return true;
}

static bool
nodeList_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	int index = args[0].toInt32();
	bool ret = nodeList_item2(ctx, hobj, index, &rval);
	args.rval().set(rval);

	return ret;
}

static bool
nodeList_item2(JSContext *ctx, JS::HandleObject hobj, int idx, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (!JS_InstanceOf(ctx, hobj, &nodeList_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	hvp.setUndefined();

	dom_nodelist *nl = (dom_nodelist *)JS::GetMaybePtrFromReservedSlot<dom_nodelist>(hobj, 0);
	dom_node *element = NULL;
	dom_exception err;

	if (!nl) {
		return true;
	}
	err = dom_nodelist_item(nl, idx, (void *)&element);

	if (err != DOM_NO_ERR || !element) {
		return true;
	}
	JSObject *obj = getElement(ctx, element);
	hvp.setObject(*obj);
	dom_node_unref(element);

	return true;
}

static bool
nodeList_set_items(JSContext *ctx, JS::HandleObject hobj, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &nodeList_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_nodelist *nl = (dom_nodelist *)(node);
	dom_exception err;
	uint32_t length, i;

	if (!nl) {
		return true;
	}
	err = dom_nodelist_get_length(nl, &length);

	if (err != DOM_NO_ERR) {
		return true;
	}

	for (i = 0; i < length; i++) {
		dom_node *element = NULL;
		err = dom_nodelist_item(nl, i, &element);

		if (err != DOM_NO_ERR || !element) {
			continue;
		}
		JSObject *obj = getElement(ctx, element);

		if (obj) {
			JS::RootedObject v(ctx, obj);
			JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));
			JS_SetElement(ctx, hobj, i, ro);
		}
		dom_node_unref(element);
	}

	return true;
}


JSObject *
getNodeList(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *el = JS_NewObject(ctx, &nodeList_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) nodeList_props);
	spidermonkey_DefineFunctions(ctx, el, nodeList_funcs);

	JS::SetReservedSlot(el, 0, JS::PrivateValue(node));
	nodeList_set_items(ctx, r_el, node);

	return el;
}
