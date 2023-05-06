/* The SpiderMonkey html element objects implementation. */

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
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/attr.h"
#include "ecmascript/spidermonkey/attributes.h"
#include "ecmascript/spidermonkey/collection.h"
#include "ecmascript/spidermonkey/element.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/keyboard.h"
#include "ecmascript/spidermonkey/nodelist.h"
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

static bool element_get_property_attributes(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_children(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_childElementCount(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_childNodes(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_className(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_className(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_firstChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_firstElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_id(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_id(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_innerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_innerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_innerText(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_lastChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_lastElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nextElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nextSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nodeName(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nodeType(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nodeValue(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_ownerDocument(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_parentElement(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_parentNode(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_previousElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_previousSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_tagName(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp);

struct listener {
	LIST_HEAD(struct listener);
	char *typ;
	JS::RootedValue fun;
};

struct element_private {
	LIST_OF(struct listener) listeners;
	struct ecmascript_interpreter *interpreter;
	JS::RootedObject thisval;
};

static std::map<void *, struct element_private *> map_privates;

static void element_finalize(JS::GCContext *op, JSObject *obj);

JSClassOps element_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	element_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass element_class = {
	"element",
	JSCLASS_HAS_RESERVED_SLOTS(2),
	&element_ops
};

JSPropertySpec element_props[] = {
	JS_PSG("attributes",	element_get_property_attributes, JSPROP_ENUMERATE),
	JS_PSG("children",	element_get_property_children, JSPROP_ENUMERATE),
	JS_PSG("childElementCount",	element_get_property_childElementCount, JSPROP_ENUMERATE),
	JS_PSG("childNodes",	element_get_property_childNodes, JSPROP_ENUMERATE),
	JS_PSGS("className",	element_get_property_className, element_set_property_className, JSPROP_ENUMERATE),
	JS_PSGS("dir",	element_get_property_dir, element_set_property_dir, JSPROP_ENUMERATE),
	JS_PSG("firstChild",	element_get_property_firstChild, JSPROP_ENUMERATE),
	JS_PSG("firstElementChild",	element_get_property_firstElementChild, JSPROP_ENUMERATE),
	JS_PSGS("id",	element_get_property_id, element_set_property_id, JSPROP_ENUMERATE),
	JS_PSGS("innerHTML",	element_get_property_innerHtml, element_set_property_innerHtml, JSPROP_ENUMERATE),
	JS_PSGS("innerText",	element_get_property_innerHtml, element_set_property_innerText, JSPROP_ENUMERATE),
	JS_PSGS("lang",	element_get_property_lang, element_set_property_lang, JSPROP_ENUMERATE),
	JS_PSG("lastChild",	element_get_property_lastChild, JSPROP_ENUMERATE),
	JS_PSG("lastElementChild",	element_get_property_lastElementChild, JSPROP_ENUMERATE),
	JS_PSG("nextElementSibling",	element_get_property_nextElementSibling, JSPROP_ENUMERATE),
	JS_PSG("nextSibling",	element_get_property_nextSibling, JSPROP_ENUMERATE),
	JS_PSG("nodeName",	element_get_property_nodeName, JSPROP_ENUMERATE),
	JS_PSG("nodeType",	element_get_property_nodeType, JSPROP_ENUMERATE),
	JS_PSG("nodeValue",	element_get_property_nodeValue, JSPROP_ENUMERATE),
	JS_PSGS("outerHTML",	element_get_property_outerHtml, element_set_property_outerHtml, JSPROP_ENUMERATE),
	JS_PSG("ownerDocument",	element_get_property_ownerDocument, JSPROP_ENUMERATE),
	JS_PSG("parentElement",	element_get_property_parentElement, JSPROP_ENUMERATE),
	JS_PSG("parentNode",	element_get_property_parentNode, JSPROP_ENUMERATE),
	JS_PSG("previousElementSibling",	element_get_property_previousElementSibling, JSPROP_ENUMERATE),
	JS_PSG("previousSibling",	element_get_property_previousSibling, JSPROP_ENUMERATE),
	JS_PSG("tagName",	element_get_property_tagName, JSPROP_ENUMERATE),
	JS_PSGS("textContent",	element_get_property_textContent, element_set_property_textContent, JSPROP_ENUMERATE),
	JS_PSGS("title",	element_get_property_title, element_set_property_title, JSPROP_ENUMERATE),
	JS_PS_END
};

static void element_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(obj, 0);
	struct element_private *el_private = JS::GetMaybePtrFromReservedSlot<struct element_private>(obj, 1);

	if (el_private) {
		map_privates.erase(el);

		struct listener *l;

		foreach(l, el_private->listeners) {
			mem_free_set(&l->typ, NULL);
		}
		free_list(el_private->listeners);
		mem_free(el_private);
	}
	if (el) {
		dom_node_unref(el);
	}
}

static bool
element_get_property_attributes(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	dom_namednodemap *attrs = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_attributes(el, &attrs);

	if (exc != DOM_NO_ERR || !attrs) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getAttributes(ctx, attrs);
	args.rval().setObject(*obj);

	return true;
}

static bool
element_get_property_children(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_get_property_childElementCount(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_get_property_childNodes(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_get_property_className(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	dom_string *classstr = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_class, &classstr);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	if (!classstr) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(classstr)));
		dom_string_unref(classstr);
	}

	return true;
}


static bool
element_get_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	dom_string *dir = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_dir, &dir);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	if (!dir) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	} else {
		if (strcmp(dom_string_data(dir), "auto") && strcmp(dom_string_data(dir), "ltr") && strcmp(dom_string_data(dir), "rtl")) {
			args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		} else {
			args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(dir)));
		}
		dom_string_unref(dir);
	}

	return true;
}

static bool
element_get_property_firstChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	args.rval().setObject(*elem);

	return true;
}

static bool
element_get_property_firstElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_get_property_id(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	dom_string *id = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_id, &id);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	if (!id) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(id)));
		dom_string_unref(id);
	}

	return true;
}

static bool
element_get_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	dom_string *lang = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_lang, &lang);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	if (!lang) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(lang)));
		dom_string_unref(lang);
	}

	return true;
}

static bool
element_get_property_lastChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	args.rval().setObject(*elem);

	return true;
}

static bool
element_get_property_lastElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_get_property_nextElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
			args.rval().setObject(*elem);
			return true;
		}
		prev_next = next;
		node = next;
	}
	args.rval().setNull();

	return true;
}

static bool
element_get_property_nodeName(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_get_property_nodeType(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_get_property_nodeValue(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_get_property_nextSibling(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	args.rval().setObject(*elem);

	return true;
}

static bool
element_get_property_ownerDocument(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_get_property_parentElement(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	args.rval().setObject(*elem);

	return true;
}

static bool
element_get_property_parentNode(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	args.rval().setObject(*elem);

	return true;
}

static bool
element_get_property_previousElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
			args.rval().setObject(*elem);
			return true;
		}
		prev_prev = prev;
		node = prev;
	}
	args.rval().setNull();

	return true;
}

static bool
element_get_property_previousSibling(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	args.rval().setObject(*elem);

	return true;
}

static bool
element_get_property_tagName(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	dom_string *tag_name = NULL;
	dom_exception exc = dom_node_get_node_name(el, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(tag_name)));
	dom_string_unref(tag_name);

	return true;
}

static bool
element_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	dom_string *title = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_title, &title);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	if (!title) {
		args.rval().setString(JS_NewStringCopyZ(ctx,  ""));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(title)));
		dom_string_unref(title);
	}

	return true;
}

static bool
dump_node_element_attribute(struct string *buf, dom_node *node)
{
	dom_exception exc;
	dom_string *attr = NULL;
	dom_string *attr_value = NULL;

	exc = dom_attr_get_name((struct dom_attr *)node, &attr);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for dom_string_create\n");
		return false;
	}

	/* Get attribute's value */
	exc = dom_attr_get_value((struct dom_attr *)node, &attr_value);
	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for element_get_attribute\n");
		dom_string_unref(attr);
		return false;
	} else if (attr_value == NULL) {
		/* Element lacks required attribute */
		dom_string_unref(attr);
		return true;
	}

	add_char_to_string(buf, ' ');
	add_bytes_to_string(buf, dom_string_data(attr), dom_string_byte_length(attr));
	add_to_string(buf, "=\"");
	add_bytes_to_string(buf, dom_string_data(attr_value), dom_string_byte_length(attr_value));
	add_char_to_string(buf, '"');

	/* Finished with the attr dom_string */
	dom_string_unref(attr);
	dom_string_unref(attr_value);

	return true;
}

static bool
dump_element(struct string *buf, dom_node *node, bool toSortAttrs)
{
// TODO toSortAttrs
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type type;
	dom_namednodemap *attrs;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for node_get_node_type\n");
		return false;
	} else {
		if (type == DOM_TEXT_NODE) {
			dom_string *str;

			exc = dom_node_get_text_content(node, &str);

			if (exc == DOM_NO_ERR && str != NULL) {
				int length = dom_string_byte_length(str);
				const char *string_text = dom_string_data(str);

				if (!((length == 1) && (*string_text == '\n'))) {
					add_bytes_to_string(buf, string_text, length);
				}
				dom_string_unref(str);
			}
			return true;
		}
		if (type != DOM_ELEMENT_NODE) {
			/* Nothing to print */
			return true;
		}
	}

	/* Get element name */
	exc = dom_node_get_node_name(node, &node_name);
	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for get_node_name\n");
		return false;
	}

	add_char_to_string(buf, '<');
	//save_in_map(mapa, node, buf->length);

	/* Get string data and print element name */
	add_bytes_to_string(buf, dom_string_data(node_name), dom_string_byte_length(node_name));

	exc = dom_node_get_attributes(node, &attrs);

	if (exc == DOM_NO_ERR) {
		dom_ulong length;

		exc = dom_namednodemap_get_length(attrs, &length);

		if (exc == DOM_NO_ERR) {
			int i;

			for (i = 0; i < length; ++i) {
				dom_node *attr;

				exc = dom_namednodemap_item(attrs, i, &attr);

				if (exc == DOM_NO_ERR) {
					dump_node_element_attribute(buf, attr);
					dom_node_unref(attr);
				}
			}
		}
		dom_node_unref(attrs);
	}
	add_char_to_string(buf, '>');

	/* Finished with the node_name dom_string */
	dom_string_unref(node_name);

	return true;
}

void
walk_tree(struct string *buf, void *nod, bool start, bool toSortAttrs)
{
	dom_node *node = (dom_node *)(nod);
	dom_nodelist *children = NULL;
	dom_node_type type;
	dom_exception exc;
	uint32_t size = 0;

	if (!start) {
		exc = dom_node_get_node_type(node, &type);

		if (exc == DOM_NO_ERR && type == DOM_TEXT_NODE) {
			dom_string *content = NULL;
			exc = dom_node_get_text_content(node, &content);

			if (exc == DOM_NO_ERR && content) {
				add_bytes_to_string(buf, dom_string_data(content), dom_string_length(content));
				dom_string_unref(content);
			}
		} else if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dump_element(buf, node, toSortAttrs);
		}
	}
	exc = dom_node_get_child_nodes(node, &children);

	if (exc == DOM_NO_ERR && children) {
		exc = dom_nodelist_get_length(children, &size);
		uint32_t i;

		for (i = 0; i < size; i++) {
			dom_node *item = NULL;
			exc = dom_nodelist_item(children, i, &item);

			if (exc == DOM_NO_ERR && item) {
				walk_tree(buf, item, false, toSortAttrs);
				dom_node_unref(item);
			}
		}
		dom_nodelist_unref(children);
	}

	if (!start) {
		exc = dom_node_get_node_type(node, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dom_string *node_name = NULL;
			exc = dom_node_get_node_name(node, &node_name);

			if (exc == DOM_NO_ERR && node_name) {
				add_to_string(buf, "</");
				add_bytes_to_string(buf, dom_string_data(node_name), dom_string_length(node_name));
				add_char_to_string(buf, '>');
				dom_string_unref(node_name);
			}
		}
	}
}

static void
walk_tree_content(struct string *buf, dom_node *node)
{
	dom_node_type type;
	dom_nodelist *children = NULL;
	dom_exception exc;

	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR && type == DOM_TEXT_NODE) {
		dom_string *content = NULL;
		exc = dom_node_get_text_content(node, &content);

		if (exc == DOM_NO_ERR && content) {
			add_bytes_to_string(buf, dom_string_data(content), dom_string_length(content));
			dom_string_unref(content);
		}
	}
	exc = dom_node_get_child_nodes(node, &children);

	if (exc == DOM_NO_ERR && children) {
		uint32_t i, size;
		exc = dom_nodelist_get_length(children, &size);

		for (i = 0; i < size; i++) {
			dom_node *item = NULL;
			exc = dom_nodelist_item(children, i, &item);

			if (exc == DOM_NO_ERR && item) {
				walk_tree_content(buf, item);
				dom_node_unref(item);
			}
		}
		dom_nodelist_unref(children);
	}
}

static bool
element_get_property_innerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	struct string buf;
	if (!init_string(&buf)) {
		args.rval().setNull();
		return false;
	}
	walk_tree(&buf, el, true, false);
	args.rval().setString(JS_NewStringCopyZ(ctx, buf.source));
	done_string(&buf);

	return true;
}

static bool
element_get_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	struct string buf;
	if (!init_string(&buf)) {
		args.rval().setNull();
		return false;
	}
	walk_tree(&buf, el, false, false);
	args.rval().setString(JS_NewStringCopyZ(ctx, buf.source));
	done_string(&buf);

	return true;
}

static bool
element_get_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	struct string buf;
	if (!init_string(&buf)) {
		args.rval().setNull();
		return false;
	}
	walk_tree_content(&buf, el);
	args.rval().setString(JS_NewStringCopyZ(ctx,buf.source));
	done_string(&buf);

	return true;
}

static bool
element_set_property_className(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return true;
	}
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	size_t len = strlen(str);
	dom_string *classstr = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, len, &classstr);

	if (exc == DOM_NO_ERR && classstr) {
		exc = dom_element_set_attribute(el, corestring_dom_class, classstr);
		interpreter->changed = true;
		dom_string_unref(classstr);
	}
	mem_free(str);

	return true;
}

static bool
element_set_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return true;
	}
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	size_t len = strlen(str);

	if (!strcmp(str, "ltr") || !strcmp(str, "rtl") || !strcmp(str, "auto")) {
		dom_string *dir = NULL;
		dom_exception exc = dom_string_create((const uint8_t *)str, len, &dir);

		if (exc == DOM_NO_ERR && dir) {
			exc = dom_element_set_attribute(el, corestring_dom_dir, dir);
			interpreter->changed = true;
			dom_string_unref(dir);
		}
	}
	mem_free(str);

	return true;
}


static bool
element_set_property_id(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return true;
	}
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	size_t len = strlen(str);
	dom_string *idstr = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, len, &idstr);

	if (exc == DOM_NO_ERR && idstr) {
		exc = dom_element_set_attribute(el, corestring_dom_id, idstr);
		interpreter->changed = true;
		dom_string_unref(idstr);
	}
	mem_free(str);

	return true;
}

static bool
element_set_property_innerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return true;
	}
	char *s = jsval_to_string(ctx, args[0]);

	if (!s) {
		return false;
	}
	size_t size = strlen(s);

	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	dom_hubbub_parser *parser = NULL;
	struct dom_document *doc = NULL;
	struct dom_document_fragment *fragment = NULL;
	dom_exception exc;
	struct dom_node *child = NULL, *html = NULL, *body = NULL;
	struct dom_nodelist *bodies = NULL;

	exc = dom_node_get_owner_document(el, &doc);
	if (exc != DOM_NO_ERR) goto out;

	parse_params.enc = "UTF-8";
	parse_params.fix_enc = true;
	parse_params.enable_script = false;
	parse_params.msg = NULL;
	parse_params.script = NULL;
	parse_params.ctx = NULL;
	parse_params.daf = NULL;

	error = dom_hubbub_fragment_parser_create(&parse_params,
						  doc,
						  &parser,
						  &fragment);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to create fragment parser!");
		goto out;
	}

	error = dom_hubbub_parser_parse_chunk(parser, (const uint8_t*)s, size);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to parse HTML chunk");
		goto out;
	}
	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to complete parser");
		goto out;
	}

	/* Parse is finished, transfer contents of fragment into node */

	/* 1. empty this node */
	exc = dom_node_get_first_child(el, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		struct dom_node *cref;
		exc = dom_node_remove_child(el, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(child);
		child = NULL;
		dom_node_unref(cref);
		exc = dom_node_get_first_child(el, &child);
		if (exc != DOM_NO_ERR) goto out;
	}

	/* 2. the first child in the fragment will be an HTML element
	 * because that's how hubbub works, walk through that to the body
	 * element hubbub will have created, we want to migrate that element's
	 * children into ourself.
	 */
	exc = dom_node_get_first_child(fragment, &html);
	if (exc != DOM_NO_ERR) goto out;

	/* We can then ask that HTML element to give us its body */
	exc = dom_element_get_elements_by_tag_name(html, corestring_dom_BODY, &bodies);
	if (exc != DOM_NO_ERR) goto out;

	/* And now we can get the body which will be the zeroth body */
	exc = dom_nodelist_item(bodies, 0, &body);
	if (exc != DOM_NO_ERR) goto out;

	/* 3. Migrate the children */
	exc = dom_node_get_first_child(body, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		struct dom_node *cref;
		exc = dom_node_remove_child(body, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(cref);
		exc = dom_node_append_child(el, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(cref);
		dom_node_unref(child);
		child = NULL;
		exc = dom_node_get_first_child(body, &child);
		if (exc != DOM_NO_ERR) goto out;
	}
out:
	if (parser != NULL) {
		dom_hubbub_parser_destroy(parser);
	}
	if (doc != NULL) {
		dom_node_unref(doc);
	}
	if (fragment != NULL) {
		dom_node_unref(fragment);
	}
	if (child != NULL) {
		dom_node_unref(child);
	}
	if (html != NULL) {
		dom_node_unref(html);
	}
	if (bodies != NULL) {
		dom_nodelist_unref(bodies);
	}
	if (body != NULL) {
		dom_node_unref(body);
	}
	mem_free(s);
	interpreter->changed = true;

	return true;
}


static bool
element_set_property_innerText(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

#if 0

// TODO
	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	dom_node *el = JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	if (!el) {
		return true;
	}

	auto children = el->get_children();
	auto it = children.begin();
	auto end = children.end();
	for (;it != end; ++it) {
		xmlpp::Node::remove_node(*it);
	}

	char *text = jsval_to_string(ctx, args[0]);
	el->add_child_text(text);
	interpreter->changed = true;
	mem_free_if(text);
#endif

	return true;
}

static bool
element_set_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return true;
	}
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	size_t len = strlen(str);
	dom_string *langstr = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, len, &langstr);

	if (exc == DOM_NO_ERR && langstr) {
		exc = dom_element_set_attribute(el, corestring_dom_lang, langstr);
		interpreter->changed = true;
		dom_string_unref(langstr);
	}
	mem_free(str);

	return true;
}

static bool
element_set_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
// TODO
	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}

static bool
element_set_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
//TODO
	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}


static bool
element_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
		interpreter->changed = true;
		dom_string_unref(titlestr);
	}
	mem_free(str);

	return true;
}

static bool element_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_appendChild(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_cloneNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_closest(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_contains(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_getAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_getAttributeNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_getElementsByTagName(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasAttributes(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasChildNodes(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_insertBefore(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_isEqualNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_isSameNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_matches(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_querySelector(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_querySelectorAll(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_remove(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_removeChild(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_replaceWith(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_setAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec element_funcs[] = {
	{ "addEventListener",	element_addEventListener,	3 },
	{ "appendChild",	element_appendChild,	1 },
	{ "cloneNode",	element_cloneNode,	1 },
	{ "closest",	element_closest,	1 },
	{ "contains",	element_contains,	1 },
	{ "getAttribute",	element_getAttribute,	1 },
	{ "getAttributeNode",	element_getAttributeNode,	1 },
	{ "getElementsByTagName",	element_getElementsByTagName,	1 },
	{ "hasAttribute",		element_hasAttribute,	1 },
	{ "hasAttributes",		element_hasAttributes,	0 },
	{ "hasChildNodes",		element_hasChildNodes,	0 },
	{ "insertBefore",		element_insertBefore,	2 },
	{ "isEqualNode",		element_isEqualNode,	1 },
	{ "isSameNode",			element_isSameNode,	1 },
	{ "matches",		element_matches,	1 },
	{ "querySelector",		element_querySelector,	1 },
	{ "querySelectorAll",		element_querySelectorAll,	1 },
	{ "remove",		element_remove,	0 },
	{ "removeChild",	element_removeChild,	1 },
	{ "removeEventListener",	element_removeEventListener,	3 },
	{ "replaceWith",	element_replaceWith,	1 },
	{ "setAttribute",	element_setAttribute,	2 },
	{ NULL }
};

#if 0
// Common part of all add_child_element*() methods.
static xmlpp::Element*
el_add_child_element_common(xmlNode* child, xmlNode* node)
{
	if (!node) {
		xmlFreeNode(child);
		throw xmlpp::internal_error("Could not add child element node");
	}
	xmlpp::Node::create_wrapper(node);

	return static_cast<xmlpp::Element*>(node->_private);
}
#endif

static void
check_contains(dom_node *node, dom_node *searched, bool *result_set, bool *result)
{
	dom_nodelist *children = NULL;
	dom_exception exc;
	uint32_t size = 0;
	uint32_t i;

	if (*result_set) {
		return;
	}
	exc = dom_node_get_child_nodes(node, &children);
	if (exc == DOM_NO_ERR && children) {
		exc = dom_nodelist_get_length(children, &size);

		for (i = 0; i < size; i++) {
			dom_node *item = NULL;
			exc = dom_nodelist_item(children, i, &item);

			if (exc == DOM_NO_ERR && item) {
				if (item == searched) {
					*result_set = true;
					*result = true;
					dom_node_unref(item);
					return;
				}
				check_contains(item, searched, result_set, result);
				dom_node_unref(item);
			}
		}
		dom_nodelist_unref(children);
	}
}

static bool
element_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_node *el = JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	struct element_private *el_private = JS::GetMaybePtrFromReservedSlot<struct element_private>(hobj, 1);

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

	struct listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (l->fun == fun) {
			args.rval().setUndefined();
			mem_free(method);
			return true;
		}
	}
	struct listener *n = (struct listener *)mem_calloc(1, sizeof(*n));

	if (n) {
		n->typ = method;
		n->fun = fun;
		add_to_list_end(el_private->listeners, n);
	}
	args.rval().setUndefined();
	return true;
}

static bool
element_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_node *el = JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	struct element_private *el_private = JS::GetMaybePtrFromReservedSlot<struct element_private>(hobj, 1);

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

	struct listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (l->fun == fun) {
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
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
element_appendChild(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_node *res = NULL;
	dom_exception exc;

	if (argc != 1) {
		args.rval().setNull();
		return true;
	}

	if (!el) {
		args.rval().setNull();
		return true;
	}
	JS::RootedObject node(ctx, &args[0].toObject());

	dom_node *el2 = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);
	exc = dom_node_append_child(el, el2, &res);

	if (exc == DOM_NO_ERR && res) {
		interpreter->changed = true;
		JSObject *obj = getElement(ctx, res);
		args.rval().setObject(*obj);
		return true;
	}
	args.rval().setNull();

	return true;
}

static bool
element_cloneNode(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	args.rval().setObject(*obj);

	return true;
}

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

static bool
element_closest(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

// TODO
#if 0
	xmlpp::Element *el = JS::GetMaybePtrFromReservedSlot<xmlpp::Element>(hobj, 0);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	struct string cssstr;
	if (!init_string(&cssstr)) {
		return false;
	}
	jshandle_value_to_char_string(&cssstr, ctx, args[0]);
	xmlpp::ustring css = cssstr.source;
	xmlpp::ustring xpath = css2xpath(css);
	done_string(&cssstr);

	xmlpp::Node::NodeSet elements;

	try {
		elements = el->find(xpath);
	} catch (xmlpp::exception &e) {
		args.rval().setNull();

		return true;
	}

	if (elements.size() == 0) {
		args.rval().setNull();
		return true;
	}

	while (el)
	{
		for (auto node: elements)
		{
			if (isAncestor(el, static_cast<xmlpp::Element *>(node)))
			{
				JSObject *elem = getElement(ctx, node);
				args.rval().setObject(*elem);

				return true;
			}
		}
		el = el->get_parent();
	}
	args.rval().setNull();
#endif
	return true;
}

static bool
element_contains(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	JS::RootedObject node(ctx, &args[0].toObject());
	dom_node *el2 = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);

	if (!el2) {
		args.rval().setBoolean(false);
		return true;
	}

	bool result_set = false;
	bool result = false;

	check_contains(el, el2, &result_set, &result);
	args.rval().setBoolean(result);

	return true;
}

static bool
element_getAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_exception exc;
	dom_string *attr_name = NULL;
	dom_string *attr_value = NULL;

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	size_t len;
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		args.rval().setNull();
		return true;
	}
	len = strlen(str);

	exc = dom_string_create((const uint8_t *)str, len, &attr_name);
	mem_free(str);

	if (exc != DOM_NO_ERR || !attr_name) {
		args.rval().setNull();
		return true;
	}

	exc = dom_element_get_attribute(el, attr_name, &attr_value);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR || !attr_value) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(attr_value)));
	dom_string_unref(attr_value);

	return true;
}

static bool
element_getAttributeNode(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_exception exc;
	dom_string *attr_name = NULL;
	dom_attr *attr = NULL;

	if (!el) {
		args.rval().setUndefined();
		return true;
	}
	size_t len;
	const char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		args.rval().setNull();
		return true;
	}
	len = strlen(str);
	exc = dom_string_create((const uint8_t *)str, len, &attr_name);
	mem_free(str);

	if (exc != DOM_NO_ERR || !attr_name) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute_node(el, attr_name, &attr);

	if (exc != DOM_NO_ERR || !attr) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getAttr(ctx, attr);
	args.rval().setObject(*obj);

	return true;
}

static bool
element_getElementsByTagName(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (argc != 1) {
		args.rval().setBoolean(false);
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setUndefined();
		return true;
	}
	char *str = jsval_to_string(ctx, args[0]);
	size_t len;

	if (!str) {
		return false;
	}
	len = strlen(str);
	dom_nodelist *nlist = NULL;
	dom_exception exc;
	dom_string *tagname = NULL;

	exc = dom_string_create((const uint8_t *)str, len, &tagname);
	mem_free(str);

	if (exc != DOM_NO_ERR || !tagname) {
		args.rval().setNull();
		return true;
	}

	exc = dom_element_get_elements_by_tag_name(el, tagname, &nlist);
	dom_string_unref(tagname);

	if (exc != DOM_NO_ERR || !nlist) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getNodeList(ctx, nlist);
	args.rval().setObject(*obj);

	return true;
}

static bool
element_hasAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	size_t slen;
	char *s = jsval_to_string(ctx, args[0]);

	if (!s) {
		args.rval().setBoolean(false);
		return true;
	}
	slen = strlen(s);
	dom_string *attr_name = NULL;
	dom_exception exc;
	bool res;
	exc = dom_string_create((const uint8_t *)s, slen, &attr_name);
	mem_free(s);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}

	exc = dom_element_has_attribute(el, attr_name, &res);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	args.rval().setBoolean(res);

	return true;
}

static bool
element_hasAttributes(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
	exc = dom_node_has_attributes(el, &res);

	if (exc != DOM_NO_ERR) {
		args.rval().setBoolean(false);
		return true;
	}
	args.rval().setBoolean(res);

	return true;
}

static bool
element_hasChildNodes(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
element_insertBefore(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
		args.rval().setBoolean(false);
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setUndefined();
		return true;
	}
	JS::RootedObject next_sibling1(ctx, &args[1].toObject());
	JS::RootedObject child1(ctx, &args[0].toObject());

	dom_node *next_sibling = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(next_sibling1, 0);

	if (!next_sibling) {
		args.rval().setNull();
		return true;
	}

	dom_node *child = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(child1, 0);

	dom_exception err;
	dom_node *spare;

	err = dom_node_insert_before(el, child, next_sibling, &spare);
	if (err != DOM_NO_ERR) {
		args.rval().setUndefined();
		return true;
	}
	JSObject *obj = getElement(ctx, spare);
	args.rval().setObject(*obj);
	interpreter->changed = true;

	return true;
}

static bool
element_isEqualNode(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	JS::RootedObject node(ctx, &args[0].toObject());
	dom_node *el2 = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);

	struct string first;
	struct string second;

	if (!init_string(&first)) {
		return false;
	}
	if (!init_string(&second)) {
		done_string(&first);
		return false;
	}

	walk_tree(&first, el, false, true);
	walk_tree(&second, el2, false, true);

	bool ret = !strcmp(first.source, second.source);

	done_string(&first);
	done_string(&second);
	args.rval().setBoolean(ret);

	return true;
}

static bool
element_isSameNode(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	JS::RootedObject node(ctx, &args[0].toObject());
	dom_node *el2 = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);
	args.rval().setBoolean(el == el2);

	return true;
}

static bool
element_matches(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

// TODO
#if 0
	xmlpp::Element *el = JS::GetMaybePtrFromReservedSlot<xmlpp::Element>(hobj, 0);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}

	struct string cssstr;
	if (!init_string(&cssstr)) {
		return false;
	}
	jshandle_value_to_char_string(&cssstr, ctx, args[0]);
	xmlpp::ustring css = cssstr.source;
	xmlpp::ustring xpath = css2xpath(css);
	done_string(&cssstr);

	xmlpp::Node::NodeSet elements;

	try {
		elements = el->find(xpath);
	} catch (xmlpp::exception &e) {
		args.rval().setBoolean(false);
		return true;
	}
	for (auto node: elements) {
		if (node == el) {
			args.rval().setBoolean(true);
			return true;
		}
	}
	args.rval().setBoolean(false);
#endif
	return true;
}


static bool
element_querySelector(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

// TODO
#if 0
	xmlpp::Element *el = JS::GetMaybePtrFromReservedSlot<xmlpp::Element>(hobj, 0);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}

	struct string cssstr;
	if (!init_string(&cssstr)) {
		return false;
	}
	jshandle_value_to_char_string(&cssstr, ctx, args[0]);
	xmlpp::ustring css = cssstr.source;
	xmlpp::ustring xpath = css2xpath(css);
	done_string(&cssstr);

	xmlpp::Node::NodeSet elements;

	try {
		elements = el->find(xpath);
	} catch (xmlpp::exception &e) {
		args.rval().setNull();
		return true;
	}

	for (auto node: elements)
	{
		if (isAncestor(el, static_cast<xmlpp::Element *>(node)))
		{
			JSObject *elem = getElement(ctx, node);

			if (elem) {
				args.rval().setObject(*elem);
				return true;
			}
		}
	}
	args.rval().setNull();
#endif
	return true;
}

static bool
element_querySelectorAll(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
// TODO
#if 0
	xmlpp::Element *el = JS::GetMaybePtrFromReservedSlot<xmlpp::Element>(hobj, 0);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}

	struct string cssstr;

	if (!init_string(&cssstr)) {
		return false;
	}
	jshandle_value_to_char_string(&cssstr, ctx, args[0]);
	xmlpp::ustring css = cssstr.source;
	xmlpp::ustring xpath = css2xpath(css);
	done_string(&cssstr);
	xmlpp::Node::NodeSet *res = new(std::nothrow) xmlpp::Node::NodeSet;

	if (!res) {
		return false;
	}

	xmlpp::Node::NodeSet elements;

	try {
		elements = el->find(xpath);
	} catch (xmlpp::exception &e) {}

	for (auto node : elements)
	{
		if (isAncestor(el, static_cast<xmlpp::Element *>(node))) {
			res->push_back(node);
		}
	}
	JSObject *elem = getCollection(ctx, res);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}
#endif
	return true;
}

static bool
element_remove(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

// TODO
#if 0
	xmlpp::Element *el = JS::GetMaybePtrFromReservedSlot<xmlpp::Element>(hobj, 0);

	if (!el) {
		return true;
	}

	xmlpp::Node::remove_node(el);
	interpreter->changed = true;
#endif
	return true;
}

static bool
element_removeChild(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
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
		interpreter->changed = true;
		JSObject *obj = getElement(ctx, spare);
		args.rval().setObject(*obj);
		return true;
	}
	args.rval().setNull();

	return true;
}

static bool
element_replaceWith(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp || argc < 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

// TODO
#if 0
	xmlpp::Element *el = JS::GetMaybePtrFromReservedSlot<xmlpp::Element>(hobj, 0);

	if (!el || !args[0].isObject()) {
		args.rval().setUndefined();
		return true;
	}

	JS::RootedObject replacement(ctx, &args[0].toObject());
	xmlpp::Node *rep = JS::GetMaybePtrFromReservedSlot<xmlpp::Node>(replacement, 0);
	auto n = xmlAddPrevSibling(el->cobj(), rep->cobj());
	xmlpp::Node::create_wrapper(n);
	xmlpp::Node::remove_node(el);
	interpreter->changed = true;
	args.rval().setUndefined();
#endif
	return true;
}

static bool
element_setAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setUndefined();
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);

	if (!el) {
		return true;
	}
	const char *attr;
	const char *value;
	size_t attr_len, value_len;
	attr = jsval_to_string(ctx, args[0]);

	if (!attr) {
		return false;
	}
	value = jsval_to_string(ctx, args[1]);

	if (!value) {
		mem_free(attr);
		return false;
	}
	attr_len = strlen(attr);
	value_len = strlen(value);

	dom_exception exc;
	dom_string *attr_str = NULL, *value_str = NULL;

	exc = dom_string_create((const uint8_t *)attr, attr_len, &attr_str);
	mem_free(attr);

	if (exc != DOM_NO_ERR || !attr_str) {
		mem_free(value);
		return false;
	}
	exc = dom_string_create((const uint8_t *)value, value_len, &value_str);
	mem_free(value);
	if (exc != DOM_NO_ERR) {
		dom_string_unref(attr_str);
		return false;
	}

	exc = dom_element_set_attribute(el,
			attr_str, value_str);
	dom_string_unref(attr_str);
	dom_string_unref(value_str);
	if (exc != DOM_NO_ERR) {
		return true;
	}
	interpreter->changed = true;

	return true;
}


JSObject *
getElement(JSContext *ctx, void *node)
{
	auto elem = map_privates.find(node);
	struct element_private *el_private = NULL;

	if (elem != map_privates.end()) {
		el_private = elem->second;
	} else {
		el_private = (struct element_private *)mem_calloc(1, sizeof(*el_private));

		if (!el_private) {
			return NULL;
		}
		init_list(el_private->listeners);
	}

	JSObject *el = JS_NewObject(ctx, &element_class);

	if (!el) {
		mem_free(el_private);
		return NULL;
	}
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	el_private->interpreter = interpreter;

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) element_props);
	spidermonkey_DefineFunctions(ctx, el, element_funcs);

	JS::SetReservedSlot(el, 0, JS::PrivateValue(node));
	JS::SetReservedSlot(el, 1, JS::PrivateValue(el_private));

	el_private->thisval = r_el;
	map_privates[node] = el_private;

	return el;
}

void
check_element_event(void *interp, void *elem, const char *event_name, struct term_event *ev)
{
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)interp;
	JSContext *ctx = (JSContext *)interpreter->backend_data;
	JSObject *obj;
	auto el = map_privates.find(elem);

	if (el == map_privates.end()) {
		return;
	}
	struct element_private *el_private = el->second;
	JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());
	JS::RootedValue r_val(ctx);
	interpreter->heartbeat = add_heartbeat(interpreter);

	struct listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, event_name)) {
			continue;
		}
		if (ev && ev->ev == EVENT_KBD && (!strcmp(event_name, "keydown") || !strcmp(event_name, "keyup"))) {
			JS::RootedValueVector argv(ctx);
			if (!argv.resize(1)) {
				return;
			}
			obj = get_keyboardEvent(ctx, ev);
			argv[0].setObject(*obj);
			JS_CallFunctionValue(ctx, el_private->thisval, l->fun, argv, &r_val);
		} else {
			JS_CallFunctionValue(ctx, el_private->thisval, l->fun, JS::HandleValueArray::empty(), &r_val);
		}
	}
	done_heartbeat(interpreter->heartbeat);

	check_for_rerender(interpreter, event_name);
}
