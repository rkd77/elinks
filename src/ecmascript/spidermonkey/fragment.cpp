/* The SpiderMonkey html DocumentFragment objects implementation. */

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
#include "document/libdom/corestrings.h"
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#include "document/libdom/renderer2.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/spidermonkey/attr.h"
#include "ecmascript/spidermonkey/attributes.h"
#include "ecmascript/spidermonkey/collection.h"
#include "ecmascript/spidermonkey/dataset.h"
#include "ecmascript/spidermonkey/domrect.h"
#include "ecmascript/spidermonkey/event.h"
#include "ecmascript/spidermonkey/element.h"
#include "ecmascript/spidermonkey/fragment.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/keyboard.h"
#include "ecmascript/spidermonkey/nodelist.h"
#include "ecmascript/spidermonkey/nodelist2.h"
#include "ecmascript/spidermonkey/style.h"
#include "ecmascript/spidermonkey/tokenlist.h"
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
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#include <iostream>
#include <algorithm>
#include <map>
#include <string>

static bool fragment_get_property_children(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_childElementCount(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_childNodes(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_firstChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_firstElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_lastChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_lastElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
//static bool fragment_get_property_nextElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_nextSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_nodeName(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_nodeType(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_nodeValue(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_ownerDocument(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_parentElement(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_parentNode(JSContext *ctx, unsigned int argc, JS::Value *vp);
//static bool fragment_get_property_previousElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_previousSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_get_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool fragment_set_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp);

struct fragment_listener {
	LIST_HEAD_EL(struct fragment_listener);
	char *typ;
	JS::Heap<JS::Value> *fun;
};

struct fragment_private {
	LIST_OF(struct fragment_listener) listeners;
	struct ecmascript_interpreter *interpreter;
	JS::Heap<JSObject *> thisval;
	dom_event_listener *listener;
	dom_node *node;
	int ref_count;
};

//static std::map<void *, struct fragment_private *> map_privates;

static void fragment_finalize(JS::GCContext *op, JSObject *obj);
static void fragment_event_handler(dom_event *event, void *pw);

JSClassOps fragment_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	fragment_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass fragment_class = {
	"DocumentFragment",
	JSCLASS_HAS_RESERVED_SLOTS(2),
	&fragment_ops
};

JSPropertySpec fragment_props[] = {
	JS_PSG("children",	fragment_get_property_children, JSPROP_ENUMERATE),
	JS_PSG("childElementCount",	fragment_get_property_childElementCount, JSPROP_ENUMERATE),
	JS_PSG("childNodes",	fragment_get_property_childNodes, JSPROP_ENUMERATE),
	JS_PSG("firstChild",	fragment_get_property_firstChild, JSPROP_ENUMERATE),
	JS_PSG("firstElementChild",	fragment_get_property_firstElementChild, JSPROP_ENUMERATE),
	JS_PSG("lastChild",	fragment_get_property_lastChild, JSPROP_ENUMERATE),
	JS_PSG("lastElementChild",	fragment_get_property_lastElementChild, JSPROP_ENUMERATE),
	JS_PSG("nextSibling",	fragment_get_property_nextSibling, JSPROP_ENUMERATE),
	JS_PSG("nodeName",	fragment_get_property_nodeName, JSPROP_ENUMERATE),
	JS_PSG("nodeType",	fragment_get_property_nodeType, JSPROP_ENUMERATE),
	JS_PSG("nodeValue",	fragment_get_property_nodeValue, JSPROP_ENUMERATE),
	JS_PSG("ownerDocument",	fragment_get_property_ownerDocument, JSPROP_ENUMERATE),
	JS_PSG("parentElement",	fragment_get_property_parentElement, JSPROP_ENUMERATE),
	JS_PSG("parentNode",	fragment_get_property_parentNode, JSPROP_ENUMERATE),
////	JS_PSG("previousElementSibling",	fragment_get_property_previousElementSibling, JSPROP_ENUMERATE),
	JS_PSG("previousSibling",	fragment_get_property_previousSibling, JSPROP_ENUMERATE),
	JS_PSGS("textContent",	fragment_get_property_textContent, fragment_set_property_textContent, JSPROP_ENUMERATE),
	JS_PS_END
};

static void fragment_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(obj, 0);
	struct fragment_private *el_private = JS::GetMaybePtrFromReservedSlot<struct fragment_private>(obj, 1);

	if (el_private) {
		if (--el_private->ref_count <= 0) {
			//map_privates.erase(el);

			if (el_private->listener) {
				dom_event_listener_unref(el_private->listener);
			}

			struct fragment_listener *l;

			foreach(l, el_private->listeners) {
				mem_free_set(&l->typ, NULL);
				delete (l->fun);
			}
			free_list(el_private->listeners);
			mem_free(el_private);
			JS::SetReservedSlot(obj, 1, JS::UndefinedValue());
		}
	}

	if (el) {
		dom_node_unref(el);
		JS::SetReservedSlot(obj, 0, JS::UndefinedValue());
	}
}


static bool
fragment_get_property_children(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_nodelist *nodes = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getNodeList(ctx, nodes);
	args.rval().setObject(*obj);

	return true;
}

static bool
fragment_get_property_childElementCount(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t res = 0;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		args.rval().setNull();
		return true;
	}
	exc = dom_nodelist_get_length(nodes, &res);
	dom_nodelist_unref(nodes);
	args.rval().setInt32(res);

	return true;
}

static bool
fragment_get_property_childNodes(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_nodelist *nodes = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getNodeList(ctx, nodes);
	args.rval().setObject(*obj);

	return true;
}


static bool
fragment_get_property_firstChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *node = NULL;
	dom_exception exc;

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_first_child(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getElement(ctx, node);
	dom_node_unref(node);
	args.rval().setObject(*elem);

	return true;
}

static bool
fragment_get_property_firstElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t size = 0;
	uint32_t i;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		args.rval().setNull();
		return true;
	}
	exc = dom_nodelist_get_length(nodes, &size);

	if (exc != DOM_NO_ERR || !size) {
		dom_nodelist_unref(nodes);
		args.rval().setNull();
		return true;
	}

	for (i = 0; i < size; i++) {
		dom_node *child = NULL;
		exc = dom_nodelist_item(nodes, i, &child);
		dom_node_type type;

		if (exc != DOM_NO_ERR || !child) {
			continue;
		}

		exc = dom_node_get_node_type(child, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dom_nodelist_unref(nodes);
			JSObject *elem = getElement(ctx, child);
			dom_node_unref(child);
			args.rval().setObject(*elem);
			return true;
		}
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	args.rval().setNull();

	return true;
}


static bool
fragment_get_property_lastChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node *last_child = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_last_child(el, &last_child);

	if (exc != DOM_NO_ERR || !last_child) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getElement(ctx, last_child);
	dom_node_unref(last_child);
	args.rval().setObject(*elem);

	return true;
}

static bool
fragment_get_property_lastElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t size = 0;
	int i;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		args.rval().setNull();
		return true;
	}
	exc = dom_nodelist_get_length(nodes, &size);

	if (exc != DOM_NO_ERR || !size) {
		dom_nodelist_unref(nodes);
		args.rval().setNull();
		return true;
	}

	for (i = size - 1; i >= 0 ; i--) {
		dom_node *child = NULL;
		exc = dom_nodelist_item(nodes, i, &child);
		dom_node_type type;

		if (exc != DOM_NO_ERR || !child) {
			continue;
		}
		exc = dom_node_get_node_type(child, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dom_nodelist_unref(nodes);
			JSObject *elem = getElement(ctx, child);
			dom_node_unref(child);
			args.rval().setObject(*elem);
			return true;
		}
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	args.rval().setNull();

	return true;
}

#if 0
static bool
fragment_get_property_nextElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node *node;
	dom_node *prev_next = NULL;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	node = el;

	while (true) {
		dom_node *next = NULL;
		dom_exception exc = dom_node_get_next_sibling(node, &next);
		dom_node_type type;

		if (prev_next) {
			dom_node_unref(prev_next);
		}

		if (exc != DOM_NO_ERR || !next) {
			args.rval().setNull();
			return true;
		}
		exc = dom_node_get_node_type(next, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			JSObject *elem = getElement(ctx, next);
			dom_node_unref(next);
			args.rval().setObject(*elem);
			return true;
		}
		prev_next = next;
		node = next;
	}
	args.rval().setNull();

	return true;
}
#endif

static bool
fragment_get_property_nodeName(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *node = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);;
	dom_string *name = NULL;
	dom_exception exc;

	if (!node) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		return true;
	}
	exc = dom_node_get_node_name(node, &name);

	if (exc != DOM_NO_ERR || !name) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(name)));
	dom_string_unref(name);

	return true;
}

static bool
fragment_get_property_nodeType(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *node = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node_type type1;
	dom_exception exc;

	if (!node) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_node_type(node, &type1);

	if (exc == DOM_NO_ERR) {
		args.rval().setInt32(type1);
		return true;
	}
	args.rval().setNull();

	return true;
}

static bool
fragment_get_property_nodeValue(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *node = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_string *content = NULL;
	dom_exception exc;

	if (!node) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_node_value(node, &content);

	if (exc != DOM_NO_ERR || !content) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(content)));
	dom_string_unref(content);

	return true;
}

static bool
fragment_get_property_nextSibling(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_next_sibling(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		args.rval().setNull();
		return true;
	}
	JSObject *elem = getElement(ctx, node);
	dom_node_unref(node);
	args.rval().setObject(*elem);

	return true;
}

static bool
fragment_get_property_ownerDocument(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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
	args.rval().setObject(*(JSObject *)(interpreter->document_obj));

	return true;
}

static bool
fragment_get_property_parentElement(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		args.rval().setNull();
		return true;
	}
	JSObject *elem = getElement(ctx, node);
	dom_node_unref(node);
	args.rval().setObject(*elem);

	return true;
}

static bool
fragment_get_property_parentNode(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		args.rval().setNull();
		return true;
	}
	JSObject *elem = getElement(ctx, node);
	dom_node_unref(node);
	args.rval().setObject(*elem);

	return true;
}

#if 0
static bool
fragment_get_property_previousElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node *node;
	dom_node *prev_prev = NULL;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	node = el;

	while (true) {
		dom_node *prev = NULL;
		dom_exception exc = dom_node_get_previous_sibling(node, &prev);
		dom_node_type type;

		if (prev_prev) {
			dom_node_unref(prev_prev);
		}

		if (exc != DOM_NO_ERR || !prev) {
			args.rval().setNull();
			return true;
		}
		exc = dom_node_get_node_type(prev, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			JSObject *elem = getElement(ctx, prev);
			dom_node_unref(prev);
			args.rval().setObject(*elem);
			return true;
		}
		prev_prev = prev;
		node = prev;
	}
	args.rval().setNull();

	return true;
}
#endif

static bool
fragment_get_property_previousSibling(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_previous_sibling(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		args.rval().setNull();
		return true;
	}
	JSObject *elem = getElement(ctx, node);
	dom_node_unref(node);
	args.rval().setObject(*elem);

	return true;
}

static bool
fragment_get_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
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
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	dom_string *content = NULL;
	dom_exception exc = dom_node_get_text_content(el, &content);

	if (exc != DOM_NO_ERR || !content) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(content)));
	dom_string_unref(content);

	return true;
}





static bool
fragment_set_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setUndefined();
		return true;
	}
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	dom_string *content = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, strlen(str), &content);
	mem_free(str);

	if (exc != DOM_NO_ERR || !content) {
		return false;
	}
	exc = dom_node_set_text_content(el, content);
	dom_string_unref(content);
	args.rval().setUndefined();

	return true;
}

#if 0
static bool
fragment_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

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
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setUndefined();

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	dom_string *titlestr = NULL;
	dom_exception exc;
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return true;
	}
	size_t len;
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	len = strlen(str);
	exc = dom_string_create((const uint8_t *)str, len, &titlestr);

	if (exc == DOM_NO_ERR && titlestr) {
		exc = dom_element_set_attribute(el, corestring_dom_title, titlestr);
		interpreter->changed = 1;
		dom_string_unref(titlestr);
		debug_dump_xhtml(document->dom);
	}
	mem_free(str);

	return true;
}
#endif


static bool fragment_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_appendChild(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_cloneNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_contains(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_dispatchEvent(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_hasChildNodes(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_insertBefore(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_isEqualNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_isSameNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_querySelector(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_querySelectorAll(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_removeChild(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool fragment_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec fragment_funcs[] = {
	{ "addEventListener",	fragment_addEventListener,	3 },
	{ "appendChild",	fragment_appendChild,	1 },
	{ "cloneNode",	fragment_cloneNode,	1 },
	{ "contains",	fragment_contains,	1 },
	{ "dispatchEvent", fragment_dispatchEvent,	1 },
	{ "hasChildNodes",		fragment_hasChildNodes,	0 },
	{ "insertBefore",		fragment_insertBefore,	2 },
	{ "isEqualNode",		fragment_isEqualNode,	1 },
	{ "isSameNode",			fragment_isSameNode,	1 },
	{ "querySelector",		fragment_querySelector,	1 },
	{ "querySelectorAll",		fragment_querySelectorAll,	1 },
	{ "removeChild",	fragment_removeChild,	1 },
	{ "removeEventListener",	fragment_removeEventListener,	3 },
	{ NULL }
};

#if 0
// Common part of all add_child_element*() methods.
static xmlpp::Element*
el_add_child_fragment_common(xmlNode* child, xmlNode* node)
{
	if (!node) {
		xmlFreeNode(child);
		throw xmlpp::internal_error("Could not add child element node");
	}
	xmlpp::Node::create_wrapper(node);

	return static_cast<xmlpp::Element*>(node->_private);
}
#endif

static bool
fragment_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_node *el = JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	struct fragment_private *el_private = JS::GetMaybePtrFromReservedSlot<struct fragment_private>(hobj, 1);

	if (!el || !el_private) {
		args.rval().setNull();
		return true;
	}

	if (argc < 2) {
		args.rval().setUndefined();
		return true;
	}
	char *method = jsval_to_string(ctx, args[0]);
	JS::RootedValue fun(ctx, args[1]);
	struct fragment_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (*(l->fun) == fun) {
			mem_free(method);
			args.rval().setUndefined();
			return true;
		}
	}
	struct fragment_listener *n = (struct fragment_listener *)mem_calloc(1, sizeof(*n));

	if (!n) {
		args.rval().setUndefined();
		return false;
	}
	n->fun = new JS::Heap<JS::Value>(fun);
	n->typ = method;
	add_to_list_end(el_private->listeners, n);
	dom_exception exc;

	if (el_private->listener) {
		dom_event_listener_ref(el_private->listener);
	} else {
		exc = dom_event_listener_create(fragment_event_handler, el_private, &el_private->listener);

		if (exc != DOM_NO_ERR || !el_private->listener) {
			args.rval().setUndefined();
			return true;
		}
	}
	dom_string *typ = NULL;
	exc = dom_string_create((const uint8_t *)method, strlen(method), &typ);

	if (exc != DOM_NO_ERR || !typ) {
		goto ex;
	}
	exc = dom_event_target_add_event_listener(el, typ, el_private->listener, false);

	if (exc == DOM_NO_ERR) {
		dom_event_listener_ref(el_private->listener);
	}

ex:
	if (typ) {
		dom_string_unref(typ);
	}
	dom_event_listener_unref(el_private->listener);
	args.rval().setUndefined();

	return true;
}

static bool
fragment_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_node *el = JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	struct fragment_private *el_private = JS::GetMaybePtrFromReservedSlot<struct fragment_private>(hobj, 1);

	if (!el || !el_private) {
		args.rval().setNull();
		return true;
	}

	if (argc < 2) {
		args.rval().setUndefined();
		return true;
	}
	char *method = jsval_to_string(ctx, args[0]);

	if (!method) {
		return false;
	}
	JS::RootedValue fun(ctx, args[1]);

	struct fragment_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (*(l->fun) == fun) {

			dom_string *typ = NULL;
			dom_exception exc = dom_string_create((const uint8_t *)method, strlen(method), &typ);

			if (exc != DOM_NO_ERR || !typ) {
				continue;
			}
			//dom_event_target_remove_event_listener(el, typ, el_private->listener, false);
			dom_string_unref(typ);

			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			delete (l->fun);
			mem_free(l);
			mem_free(method);
			args.rval().setUndefined();
			return true;
		}
	}
	mem_free(method);
	args.rval().setUndefined();
	return true;
}

static bool
fragment_appendChild(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node *res = NULL;

	if (argc != 1) {
		return false;
	}

	if (!el) {
		return false;
	}
	dom_node *el2 = NULL;

	if (!args[0].isNull()) {
		JS::RootedObject node(ctx, &args[0].toObject());
		el2 = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);
	}

	if (!el2) {
		return false;
	}
	dom_exception exc = dom_node_append_child(el, el2, &res);

	if (exc == DOM_NO_ERR && res) {
		interpreter->changed = 1;
		JSObject *obj = getElement(ctx, res);
		dom_node_unref(res);
		args.rval().setObject(*obj);
		debug_dump_xhtml(document->dom);
		return true;
	}

	return false;
}


static bool
fragment_cloneNode(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	dom_exception exc;
	bool deep = args[0].toBoolean();
	dom_node *clone = NULL;
	exc = dom_node_clone_node(el, deep, &clone);

	if (exc != DOM_NO_ERR || !clone) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getElement(ctx, clone);
	dom_node_unref(clone);
	args.rval().setObject(*obj);

	return true;
}
#if 0
static bool
isAncestor(dom_node *el, dom_node *node)
{
	dom_node *prev_next = NULL;
	while (node) {
		dom_exception exc;
		dom_node *next = NULL;
		if (prev_next) {
			dom_node_unref(prev_next);
		}
		if (el == node) {
			return true;
		}
		exc = dom_node_get_parent_node(node, &next);
		if (exc != DOM_NO_ERR || !next) {
			break;
		}
		prev_next = next;
		node = next;
	}

	return false;
}
#endif


static bool
fragment_contains(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return false;
	}
	dom_node *el2 = NULL;

	if (!args[0].isNull()) {
		JS::RootedObject node(ctx, &args[0].toObject());
		el2 = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);
	}

	if (!el2) {
		return false;
	}
	dom_node_ref(el);
	dom_node_ref(el2);

	while (1) {
		if (el == el2) {
			dom_node_unref(el);
			dom_node_unref(el2);
			args.rval().setBoolean(true);
			return true;
		}
		dom_node *node = NULL;
		dom_exception exc = dom_node_get_parent_node(el2, &node);

		if (exc != DOM_NO_ERR || !node) {
			dom_node_unref(el);
			dom_node_unref(el2);
			args.rval().setBoolean(false);
			return true;
		}
		dom_node_unref(el2);
		el2 = node;
	}
}


static bool
fragment_hasChildNodes(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc != 0) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
		args.rval().setBoolean(false);
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_exception exc;
	bool res;

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	exc = dom_node_has_child_nodes(el, &res);

	if (exc != DOM_NO_ERR) {
		args.rval().setBoolean(false);
		return true;
	}
	args.rval().setBoolean(res);

	return true;
}

static bool
fragment_insertBefore(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc != 2) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return false;
	}
	dom_node *next_sibling = NULL;
	dom_node *child = NULL;

	if (!args[1].isNull()) {
		JS::RootedObject next_sibling1(ctx, &args[1].toObject());
		next_sibling = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(next_sibling1, 0);
	}

	if (!args[0].isNull()) {
		JS::RootedObject child1(ctx, &args[0].toObject());
		child = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(child1, 0);
	}

	if (!child) {
		return false;
	}
	dom_node *spare = NULL;
	dom_exception err = dom_node_insert_before(el, child, next_sibling, &spare);

	if (err != DOM_NO_ERR || !spare) {
		return false;
	}
	JSObject *obj = getElement(ctx, spare);
	dom_node_unref(spare);
	args.rval().setObject(*obj);
	interpreter->changed = 1;
	debug_dump_xhtml(document->dom);

	return true;
}

static bool
fragment_isEqualNode(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return false;
	}
	dom_node *el2 = NULL;

	if (!args[0].isNull()) {
		JS::RootedObject node(ctx, &args[0].toObject());
		el2 = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);
	}

	if (!el2) {
		return false;
	}

	struct string first;
	struct string second;

	if (!init_string(&first)) {
		return false;
	}
	if (!init_string(&second)) {
		done_string(&first);
		return false;
	}

	ecmascript_walk_tree(&first, el, false, true);
	ecmascript_walk_tree(&second, el2, false, true);

	bool ret = !strcmp(first.source, second.source);

	done_string(&first);
	done_string(&second);
	args.rval().setBoolean(ret);

	return true;
}

static bool
fragment_isSameNode(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return false;
	}
	dom_node *el2 = NULL;

	if (!args[0].isNull()) {
		JS::RootedObject node(ctx, &args[0].toObject());
		el2 = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);
	}
	args.rval().setBoolean(el == el2);

	return true;
}

static bool
fragment_querySelector(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	char *selector = jsval_to_string(ctx, args[0]);

	if (!selector) {
		args.rval().setNull();
		return true;
	}
	void *ret = walk_tree_query(el, selector, 0);
	mem_free(selector);

	if (!ret) {
		args.rval().setNull();
	} else {
		JSObject *el = getElement(ctx, ret);
		dom_node_unref(ret);
		args.rval().setObject(*el);
	}

	return true;
}

static bool
fragment_querySelectorAll(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	char *selector = jsval_to_string(ctx, args[0]);

	if (!selector) {
		args.rval().setNull();
		return true;
	}
	LIST_OF(struct selector_node) *result_list = (LIST_OF(struct selector_node) *)mem_calloc(1, sizeof(*result_list));

	if (!result_list) {
		mem_free(selector);
		args.rval().setNull();
		return true;
	}
	init_list(*result_list);
	walk_tree_query_append(el, selector, 0, result_list);
	mem_free(selector);
	JSObject *obj = getNodeList2(ctx, result_list);
	args.rval().setObject(*obj);

	return true;
}

static bool
fragment_removeChild(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el || !args[0].isObject()) {
		args.rval().setNull();
		return true;
	}
	JS::RootedObject node(ctx, &args[0].toObject());
	dom_node *el2 = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);
	dom_exception exc;
	dom_node *spare = NULL;
	exc = dom_node_remove_child(el, el2, &spare);

	if (exc == DOM_NO_ERR && spare) {
		interpreter->changed = 1;
		JSObject *obj = getElement(ctx, spare);
		dom_node_unref(spare);
		args.rval().setObject(*obj);
		debug_dump_xhtml(document->dom);
		return true;
	}
	args.rval().setNull();

	return true;
}

JSObject *
getDocumentFragment(JSContext *ctx, void *node)
{
	struct fragment_private *el_private = (struct fragment_private *)mem_calloc(1, sizeof(*el_private));

	if (!el_private) {
		return NULL;
	}
	init_list(el_private->listeners);
	el_private->ref_count = 1;
	el_private->node = (dom_node *)node;

	JSObject *el = JS_NewObject(ctx, &fragment_class);

	if (!el) {
		mem_free(el_private);
		return NULL;
	}
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	el_private->interpreter = interpreter;

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) fragment_props);
	spidermonkey_DefineFunctions(ctx, el, fragment_funcs);

	JS::SetReservedSlot(el, 0, JS::PrivateValue(node));
	JS::SetReservedSlot(el, 1, JS::PrivateValue(el_private));

	el_private->thisval = r_el;
	dom_node_ref((dom_node *)node);

	return el;
}

#if 0
void
check_fragment_event(void *interp, void *elem, const char *event_name, struct term_event *ev)
{
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)interp;
	JSContext *ctx = (JSContext *)interpreter->backend_data;
	JSObject *obj;
	auto el = map_privates.find(elem);

	if (el == map_privates.end()) {
		return;
	}
	struct fragment_private *el_private = el->second;
	JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());
	JS::RootedValue r_val(ctx);
	interpreter->heartbeat = add_heartbeat(interpreter);

	struct fragment_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, event_name)) {
			continue;
		}
		if (ev && ev->ev == EVENT_KBD && (!strcmp(event_name, "keydown") || !strcmp(event_name, "keyup") || !strcmp(event_name, "keypress"))) {
			JS::RootedValueArray<1> argv(ctx);
			obj = get_keyboardEvent(ctx, ev);
			argv[0].setObject(*obj);
			JS::RootedValue r_val(ctx);
			JS::RootedObject thisv(ctx, el_private->thisval);
			JS::RootedValue vfun(ctx, *(l->fun));
			JS_CallFunctionValue(ctx, thisv, vfun, argv, &r_val);
		} else {
			JS::RootedValue r_val(ctx);
			JS::RootedObject thisv(ctx, el_private->thisval);
			JS::RootedValue vfun(ctx, *(l->fun));
			JS_CallFunctionValue(ctx, thisv, vfun, JS::HandleValueArray::empty(), &r_val);
		}
	}
	done_heartbeat(interpreter->heartbeat);

	check_for_rerender(interpreter, event_name);
}
#endif

static bool
fragment_dispatchEvent(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &fragment_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *element = JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!element) {
		args.rval().setBoolean(false);
		return true;
	}

	if (argc < 1) {
		args.rval().setBoolean(false);
		return true;
	}
	JS::RootedObject eve(ctx, &args[0].toObject());
	dom_event *event = (dom_event *)JS::GetMaybePtrFromReservedSlot<dom_event>(eve, 0);
	bool result = false;
	(void)dom_event_target_dispatch_event(element, event, &result);
	args.rval().setBoolean(result);

	return true;
}

static void
fragment_event_handler(dom_event *event, void *pw)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct fragment_private *el_private = (struct fragment_private *)pw;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)el_private->interpreter;
	JSContext *ctx = (JSContext *)interpreter->backend_data;
	JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());
	JS::RootedValue r_val(ctx);

	if (!event) {
		return;
	}
	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		return;
	}
	interpreter->heartbeat = add_heartbeat(interpreter);
	JSObject *obj_ev = getEvent(ctx, event);
	interpreter->heartbeat = add_heartbeat(interpreter);

	struct fragment_listener *l, *next;

	foreachsafe(l, next, el_private->listeners) {
		if (strcmp(l->typ, dom_string_data(typ))) {
			continue;
		}
		JS::RootedValueArray<1> argv(ctx);
		argv[0].setObject(*obj_ev);
		JS::RootedValue r_val(ctx);
		JS::RootedObject thisv(ctx, el_private->thisval);
		JS::RootedValue vfun(ctx, *(l->fun));
		JS_CallFunctionValue(ctx, thisv, vfun, argv, &r_val);
	}
	done_heartbeat(interpreter->heartbeat);
	check_for_rerender(interpreter, dom_string_data(typ));
	dom_string_unref(typ);
}
