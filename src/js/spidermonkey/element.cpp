/* The SpiderMonkey html element objects implementation. */

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
#include "document/libdom/corestrings.h"
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#include "document/libdom/renderer2.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/ecmascript-c.h"
#include "js/spidermonkey/attr.h"
#include "js/spidermonkey/attributes.h"
#include "js/spidermonkey/collection.h"
#include "js/spidermonkey/document.h"
#include "js/spidermonkey/dataset.h"
#include "js/spidermonkey/domrect.h"
#include "js/spidermonkey/event.h"
#include "js/spidermonkey/element.h"
#include "js/spidermonkey/heartbeat.h"
#include "js/spidermonkey/keyboard.h"
#include "js/spidermonkey/node.h"
#include "js/spidermonkey/nodelist.h"
#include "js/spidermonkey/nodelist2.h"
#include "js/spidermonkey/style.h"
#include "js/spidermonkey/tokenlist.h"
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
#include <map>
#include <string>

static bool element_get_property_attributes(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_checked(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_checked(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_children(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_childElementCount(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_childNodes(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_classList(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_className(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_className(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_contentDocument(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_contentWindow(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_dataset(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_clientHeight(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_clientLeft(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_clientTop(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_clientWidth(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_firstChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_firstElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_href(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_href(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_id(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_id(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_innerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_innerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_innerText(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_lastChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_lastElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nextElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nextSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nodeName(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nodeType(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nodeValue(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_nodeValue(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_offsetHeight(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_offsetLeft(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_offsetParent(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_offsetTop(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_offsetWidth(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_ownerDocument(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_parentElement(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_parentNode(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_previousElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_previousSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_src(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_style(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_tagName(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_width(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp);


struct ele_listener {
	LIST_HEAD_EL(struct ele_listener);
	char *typ;
	JS::Heap<JS::Value> *fun;
};

struct element_private {
	LIST_OF(struct ele_listener) listeners;
	struct ecmascript_interpreter *interpreter;
	JS::Heap<JSObject *> thisval;
	dom_event_listener *listener;
	dom_node *node;
	int ref_count;
};

static std::map<void *, struct element_private *> map_privates;

static void element_finalize(JS::GCContext *op, JSObject *obj);

static void element_event_handler(dom_event *event, void *pw);

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
	JS_PSGS("checked",	element_get_property_checked, element_set_property_checked, JSPROP_ENUMERATE),
	JS_PSG("children",	element_get_property_children, JSPROP_ENUMERATE),
	JS_PSG("childElementCount",	element_get_property_childElementCount, JSPROP_ENUMERATE),
	JS_PSG("childNodes",	element_get_property_childNodes, JSPROP_ENUMERATE),
	JS_PSG("classList",	element_get_property_classList, JSPROP_ENUMERATE),
	JS_PSGS("className",	element_get_property_className, element_set_property_className, JSPROP_ENUMERATE),
	JS_PSG("contentDocument",	element_get_property_contentDocument, JSPROP_ENUMERATE),
	JS_PSG("contentWindow",	element_get_property_contentWindow, JSPROP_ENUMERATE),
	JS_PSG("clientHeight",	element_get_property_clientHeight, JSPROP_ENUMERATE),
	JS_PSG("clientLeft",	element_get_property_clientLeft, JSPROP_ENUMERATE),
	JS_PSG("clientTop",	element_get_property_clientTop, JSPROP_ENUMERATE),
	JS_PSG("clientWidth",	element_get_property_clientWidth, JSPROP_ENUMERATE),
	JS_PSG("dataset",	element_get_property_dataset, JSPROP_ENUMERATE),
	JS_PSGS("dir",	element_get_property_dir, element_set_property_dir, JSPROP_ENUMERATE),
	JS_PSG("firstChild",	element_get_property_firstChild, JSPROP_ENUMERATE),
	JS_PSG("firstElementChild",	element_get_property_firstElementChild, JSPROP_ENUMERATE),
	JS_PSG("height",		element_get_property_height, JSPROP_ENUMERATE),
	JS_PSGS("href",	element_get_property_href, element_set_property_href, JSPROP_ENUMERATE),
	JS_PSGS("id",	element_get_property_id, element_set_property_id, JSPROP_ENUMERATE),
	JS_PSGS("innerHTML",	element_get_property_innerHtml, element_set_property_innerHtml, JSPROP_ENUMERATE),
	JS_PSGS("innerText",	element_get_property_innerHtml, element_set_property_innerText, JSPROP_ENUMERATE),
	JS_PSGS("lang",	element_get_property_lang, element_set_property_lang, JSPROP_ENUMERATE),
	JS_PSG("lastChild",	element_get_property_lastChild, JSPROP_ENUMERATE),
	JS_PSG("lastElementChild",	element_get_property_lastElementChild, JSPROP_ENUMERATE),
	JS_PSG("name", element_get_property_name, JSPROP_ENUMERATE),
	JS_PSG("nextElementSibling",	element_get_property_nextElementSibling, JSPROP_ENUMERATE),
	JS_PSG("nextSibling",	element_get_property_nextSibling, JSPROP_ENUMERATE),
	JS_PSG("nodeName",	element_get_property_nodeName, JSPROP_ENUMERATE),
	JS_PSG("nodeType",	element_get_property_nodeType, JSPROP_ENUMERATE),
	JS_PSGS("nodeValue",	element_get_property_nodeValue, element_set_property_nodeValue, JSPROP_ENUMERATE),
	JS_PSG("offsetHeight",	element_get_property_offsetHeight, JSPROP_ENUMERATE),
	JS_PSG("offsetLeft",	element_get_property_offsetLeft, JSPROP_ENUMERATE),
	JS_PSG("offsetParent",	element_get_property_offsetParent, JSPROP_ENUMERATE),
	JS_PSG("offsetTop",	element_get_property_offsetTop, JSPROP_ENUMERATE),
	JS_PSG("offsetWidth",	element_get_property_offsetWidth, JSPROP_ENUMERATE),
	JS_PSGS("outerHTML",	element_get_property_outerHtml, element_set_property_outerHtml, JSPROP_ENUMERATE),
	JS_PSG("ownerDocument",	element_get_property_ownerDocument, JSPROP_ENUMERATE),
	JS_PSG("parentElement",	element_get_property_parentElement, JSPROP_ENUMERATE),
	JS_PSG("parentNode",	element_get_property_parentNode, JSPROP_ENUMERATE),
	JS_PSG("previousElementSibling",	element_get_property_previousElementSibling, JSPROP_ENUMERATE),
	JS_PSG("previousSibling",	element_get_property_previousSibling, JSPROP_ENUMERATE),
	JS_PSG("src", element_get_property_src, JSPROP_ENUMERATE),
	JS_PSG("style",		element_get_property_style, JSPROP_ENUMERATE),
	JS_PSG("tagName",	element_get_property_tagName, JSPROP_ENUMERATE),
	JS_PSGS("textContent",	element_get_property_textContent, element_set_property_textContent, JSPROP_ENUMERATE),
	JS_PSGS("title",	element_get_property_title, element_set_property_title, JSPROP_ENUMERATE),
	JS_PSGS("value",	element_get_property_value, element_set_property_value, JSPROP_ENUMERATE),
	JS_PSG("width",		element_get_property_width, JSPROP_ENUMERATE),
	JS_PS_END
};

void
unset_el_object(void *data)
{
	JSObject *obj = (JSObject *)data;

	if (obj) {
		JS::SetReservedSlot(obj, 0, JS::UndefinedValue());
	}
}

static void element_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(obj, 0);
	NODEINFO(el);
	struct element_private *el_private = JS::GetMaybePtrFromReservedSlot<struct element_private>(obj, 1);

	if (el_private) {
		if (--el_private->ref_count <= 0) {
			map_privates.erase(el);

			if (el_private->listener) {
				dom_event_listener_unref(el_private->listener);
			}

			struct ele_listener *l;

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
		void *old_node_data = NULL;
		dom_node_set_user_data(el, corestring_dom___ns_key_html_content_data, NULL, js_html_document_user_data_handler,
			(void *) &old_node_data);
		dom_node_unref(el);
		JS::SetReservedSlot(obj, 0, JS::UndefinedValue());
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
	NODEINFO(el);
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
element_get_property_checked(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct view_state *vs = interpreter->vs;

	args.rval().setUndefined();

	if (!vs) {
		return true;
	}

	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return true;
	}

	struct document *doc = doc_view->document;

	if (!doc) {
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		return true;
	}
	int offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		return true;
	}

	int linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		return true;
	}
	struct link *link = &doc->links[linknum];
	struct el_form_control *fc = get_link_form_control(link);

	if (!fc) {
		return true;
	}
	struct form_state *fs = find_form_state(doc_view, fc);

	if (!fs) {
		return true;
	}
	args.rval().setBoolean(fs->state);

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
	NODEINFO(el);
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
	NODEINFO(el);
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
	NODEINFO(el);
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
element_get_property_classList(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_element *el = (dom_element *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	dom_tokenlist *tl = NULL;
	dom_exception exc = dom_tokenlist_create(el, corestring_dom_class, &tl);

	if (exc != DOM_NO_ERR || !tl) {
		args.rval().setNull();
		return true;
	}
	JSObject *res = getTokenlist(ctx, tl);
	args.rval().setObject(*res);
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

	dom_html_element *el = (dom_html_element *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);
	dom_string *classstr = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_html_element_get_class_name(el, &classstr);

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
element_get_property_contentDocument(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_html_element *el = (dom_html_element *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	dom_html_element_type ty;
	exc = dom_html_element_get_tag_type(el, &ty);

	if (exc == DOM_NO_ERR && ty == DOM_HTML_ELEMENT_TYPE_IFRAME) {
		struct document_view *doc_view = vs->doc_view;
		struct session *ses = doc_view->session;

		if (!ses) {
			args.rval().setUndefined();
			return true;
		}
		dom_string *name = NULL;
		exc = dom_html_iframe_element_get_name((dom_html_iframe_element *)el, &name);

		if (exc != DOM_NO_ERR || !name) {
			args.rval().setUndefined();
			return true;
		}
		char *iframe_name = memacpy(dom_string_data(name), dom_string_length(name));
		dom_string_unref(name);

		if (!iframe_name) {
			args.rval().setUndefined();
			return true;
		}
		struct document_view *doc_view2 = NULL;

		foreachback (doc_view, ses->scrn_iframes) {
			if (!c_strcasecmp(doc_view->name, iframe_name)) {
				doc_view2 = doc_view;
				break;
			}
		}
		mem_free(iframe_name);

		if (!doc_view2) {
			args.rval().setUndefined();
			return true;
		}
		struct document *document = doc_view2->document;

		if (!document) {
			args.rval().setUndefined();
			return true;
		}

		if (document->dom) {
			JSObject *doc = getDocument(ctx, document->dom);
			args.rval().setObject(*doc);
			return true;
		}
	}
	args.rval().setUndefined();
	return true;
}

static bool
element_get_property_contentWindow(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_html_element *el = (dom_html_element *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	dom_html_element_type ty;
	exc = dom_html_element_get_tag_type(el, &ty);

	if (exc == DOM_NO_ERR && ty == DOM_HTML_ELEMENT_TYPE_IFRAME) {
		struct document_view *doc_view = vs->doc_view;
		struct session *ses = doc_view->session;

		if (!ses) {
			args.rval().setUndefined();
			return true;
		}
		struct location *loc = cur_loc(ses);
		if (!loc) {
			args.rval().setUndefined();
			return true;
		}
		dom_string *name = NULL;
		exc = dom_html_iframe_element_get_name((dom_html_iframe_element *)el, &name);

		if (exc != DOM_NO_ERR || !name) {
			args.rval().setUndefined();
			return true;
		}
		char *iframe_name = memacpy(dom_string_data(name), dom_string_length(name));
		dom_string_unref(name);

		if (!iframe_name) {
			args.rval().setUndefined();
			return true;
		}
		struct frame *iframe = NULL;
		struct frame *iframe2 = NULL;

		foreach (iframe2, loc->iframes) {
			if (!c_strcasecmp(iframe2->name, iframe_name)) {
				iframe = iframe2;
				break;
			}
		}
		mem_free(iframe_name);

		if (!iframe) {
			args.rval().setUndefined();
			return true;
		}
		struct view_state *ifvs = &iframe->vs;
		JSObject *contentWindow = getWindow(ctx, ifvs);
		args.rval().setObject(*contentWindow);

		return true;
	}
	args.rval().setUndefined();
	return true;
}

static bool
element_get_property_dataset(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getDataset(ctx, el);
	args.rval().setObject(*obj);

	return true;
}

// it does not work yet
static bool
element_get_property_clientHeight(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		args.rval().setInt32(0);
		return true;
	}
	ses = doc_view->session;

	if (!ses) {
		args.rval().setInt32(0);
		return true;
	}

	dom_html_element_type ty;
	dom_exception exc = dom_html_element_get_tag_type(el, &ty);

	if (exc != DOM_NO_ERR) {
		args.rval().setInt32(0);
		return true;
	}
	bool root = (ty == DOM_HTML_ELEMENT_TYPE_BODY || ty == DOM_HTML_ELEMENT_TYPE_HTML);

	if (root) {
		int height = doc_view->box.height * ses->tab->term->cell_height;
		args.rval().setInt32(height);
		return true;
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		args.rval().setInt32(ses->tab->term->cell_height);
		return true;
	}
	scan_document(document);
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		args.rval().setInt32(ses->tab->term->cell_height);
		return true;
	}
	int dy = int_max(0, (rect->y1 + 1 - rect->y0) * ses->tab->term->cell_height);
	args.rval().setInt32(dy);

	return true;
}

static bool
element_get_property_clientLeft(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(0);

	return true;
}

static bool
element_get_property_clientTop(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(0);

	return true;
}

static bool
element_get_property_clientWidth(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		args.rval().setInt32(0);
		return true;
	}
	ses = doc_view->session;

	if (!ses) {
		args.rval().setInt32(0);
		return true;
	}
	dom_html_element_type ty;
	dom_exception exc = dom_html_element_get_tag_type(el, &ty);

	if (exc != DOM_NO_ERR) {
		args.rval().setInt32(0);
		return true;
	}
	bool root = (ty == DOM_HTML_ELEMENT_TYPE_BODY || ty == DOM_HTML_ELEMENT_TYPE_HTML || ty == DOM_HTML_ELEMENT_TYPE_DIV);

	if (root) {
		int width = doc_view->box.width * ses->tab->term->cell_width;
		args.rval().setInt32(width);
		return true;
	}
	bool pre = (ty == DOM_HTML_ELEMENT_TYPE_PRE);

	if (pre) {
		dom_string *id = NULL;
		exc = dom_element_get_attribute(el, corestring_dom_id, &id);

		if (exc == DOM_NO_ERR && id != NULL) {
			if (!strcmp(dom_string_data(id), "cursor")) {
				args.rval().setInt32(ses->tab->term->cell_width);
				dom_string_unref(id);
				return true;
			}
			if (!strcmp(dom_string_data(id), "console")) {
				args.rval().setInt32(doc_view->box.width * ses->tab->term->cell_width);
				dom_string_unref(id);
				return true;
			}
			dom_string_unref(id);
		}
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		args.rval().setInt32(ses->tab->term->cell_width);
		return true;
	}
	scan_document(document);
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		args.rval().setInt32(ses->tab->term->cell_width);
		return true;
	}
	int dx = int_max(0, (rect->x1 + 1 - rect->x0) * ses->tab->term->cell_width);
	args.rval().setInt32(dx);

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
	NODEINFO(el);
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
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_node_get_first_child(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getNode(ctx, node);
	dom_node_unref(node);
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
	NODEINFO(el);
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
			JSObject *elem = getNode(ctx, child);
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
element_get_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);
	dom_string *h = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_height, &h);

	if (exc != DOM_NO_ERR || !h) {
		args.rval().setInt32(0);
		return true;
	}
	int height = atoi(dom_string_data(h));
	args.rval().setInt32(height);
	dom_string_unref(h);

	return true;
}

static bool
element_get_property_href(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);
	dom_string *href = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_href, &href);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	if (!href) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(href)));
		dom_string_unref(href);
	}

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
	NODEINFO(el);
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
	NODEINFO(el);
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
	NODEINFO(el);
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

	JSObject *elem = getNode(ctx, last_child);
	dom_node_unref(last_child);
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
	NODEINFO(el);
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
			JSObject *elem = getNode(ctx, child);
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
element_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);
	dom_string *name = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_name, &name);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	if (!name) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(name)));
		dom_string_unref(name);
	}

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
	NODEINFO(el);
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

		dom_node_unref(prev_next);

		if (exc != DOM_NO_ERR || !next) {
			args.rval().setNull();
			return true;
		}
		exc = dom_node_get_node_type(next, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			JSObject *elem = getNode(ctx, next);
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
	NODEINFO(node);
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
	NODEINFO(node);

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
	NODEINFO(node);

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
element_set_property_nodeValue(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(node);

	args.rval().setUndefined();

	if (!node) {
		return true;
	}
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	dom_string *value = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, strlen(str), &value);
	mem_free(str);

	if (exc != DOM_NO_ERR || !value) {
		return true;
	}
	exc = dom_node_set_node_value(node, value);
	dom_string_unref(value);
	interpreter->changed = true;

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
	NODEINFO(el);

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
	JSObject *elem = getNode(ctx, node);
	dom_node_unref(node);
	args.rval().setObject(*elem);

	return true;
}

static bool
element_get_property_offsetHeight(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return element_get_property_clientHeight(ctx, argc, vp);
}

static bool
element_get_property_offsetLeft(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		args.rval().setInt32(0);
		return true;
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		args.rval().setInt32(0);
		return true;
	}
	scan_document(document);
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		args.rval().setInt32(0);
		return true;
	}
	ses = doc_view->session;

	if (!ses) {
		args.rval().setInt32(0);
		return true;
	}
	dom_node *node = NULL;
	dom_exception exc = dom_node_get_parent_node(el, &node);
	if (exc != DOM_NO_ERR || !node) {
		args.rval().setInt32(0);
		return true;
	}
	int offset_parent = find_offset(document->element_map_rev, node);

	if (offset_parent <= 0) {
		args.rval().setInt32(0);
		return true;
	}
	struct node_rect *rect_parent = get_element_rect(document, offset_parent);

	if (!rect_parent) {
		args.rval().setInt32(0);
		return true;
	}
	int dx = int_max(0, (rect->x0 - rect_parent->x0) * ses->tab->term->cell_width);
	dom_node_unref(node);
	args.rval().setInt32(dx);

	return true;
}

static bool
element_get_property_offsetParent(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);

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
	JSObject *elem = getNode(ctx, node);
	dom_node_unref(node);
	args.rval().setObject(*elem);

	return true;
}

static bool
element_get_property_offsetTop(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		args.rval().setInt32(0);
		return true;
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		args.rval().setInt32(0);
		return true;
	}
	scan_document(document);
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		args.rval().setInt32(0);
		return true;
	}
	ses = doc_view->session;

	if (!ses) {
		args.rval().setInt32(0);
		return true;
	}
	dom_node *node = NULL;
	dom_exception exc = dom_node_get_parent_node(el, &node);
	if (exc != DOM_NO_ERR || !node) {
		args.rval().setInt32(0);
		return true;
	}
	int offset_parent = find_offset(document->element_map_rev, node);

	if (offset_parent <= 0) {
		args.rval().setInt32(0);
		return true;
	}
	struct node_rect *rect_parent = get_element_rect(document, offset_parent);

	if (!rect_parent) {
		args.rval().setInt32(0);
		return true;
	}
	int dy = int_max(0, (rect->y0 - rect_parent->y0) * ses->tab->term->cell_height);
	dom_node_unref(node);
	args.rval().setInt32(dy);

	return true;
}

static bool
element_get_property_offsetWidth(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return element_get_property_clientWidth(ctx, argc, vp);
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
	NODEINFO(el);

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
	JSObject *elem = getNode(ctx, node);
	dom_node_unref(node);
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
	NODEINFO(el);

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
	JSObject *elem = getNode(ctx, node);
	dom_node_unref(node);
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
	NODEINFO(el);

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

		dom_node_unref(prev_prev);

		if (exc != DOM_NO_ERR || !prev) {
			args.rval().setNull();
			return true;
		}
		exc = dom_node_get_node_type(prev, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			JSObject *elem = getNode(ctx, prev);
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
	NODEINFO(el);

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
	JSObject *elem = getNode(ctx, node);
	dom_node_unref(node);
	args.rval().setObject(*elem);

	return true;
}

static bool
element_get_property_src(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);

	dom_string *src = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_src, &src);

	if (exc != DOM_NO_ERR) {
		args.rval().setNull();
		return true;
	}
	if (!src) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(src)));
		dom_string_unref(src);
	}

	return true;
}

static bool
element_get_property_style(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	JSObject *style = getStyle(ctx, el);
	args.rval().setObject(*style);

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
	NODEINFO(el);

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
	NODEINFO(el);

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
element_get_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct view_state *vs = interpreter->vs;

	args.rval().setUndefined();

	if (!vs) {
		return true;
	}

	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return true;
	}

	struct document *doc = doc_view->document;

	if (!doc) {
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		return true;
	}
	int offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		return true;
	}

	int linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		return true;
	}
	struct link *link = &doc->links[linknum];
	struct el_form_control *fc = get_link_form_control(link);

	if (!fc) {
		return true;
	}
	struct form_state *fs = find_form_state(doc_view, fc);

	if (!fs) {
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, fs->value));

	return true;
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
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	struct string buf;
	if (!init_string(&buf)) {
		args.rval().setNull();
		return false;
	}
	ecmascript_walk_tree(&buf, el, true, false);
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
	NODEINFO(el);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	struct string buf;
	if (!init_string(&buf)) {
		args.rval().setNull();
		return false;
	}
	ecmascript_walk_tree(&buf, el, false, false);
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
	NODEINFO(el);

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
element_get_property_width(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	NODEINFO(el);

	dom_string *w = NULL;
	dom_exception exc;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_width, &w);

	if (exc != DOM_NO_ERR || !w) {
		args.rval().setInt32(0);
		return true;
	}
	int width = atoi(dom_string_data(w));
	args.rval().setInt32(width);
	dom_string_unref(w);

	return true;
}


static bool
element_set_property_checked(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct view_state *vs = interpreter->vs;

	args.rval().setUndefined();

	if (!vs) {
		return true;
	}

	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return true;
	}

	struct document *doc = doc_view->document;

	if (!doc) {
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		return true;
	}
	int offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		return true;
	}

	int linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		return true;
	}
	struct link *link = &doc->links[linknum];
	struct el_form_control *fc = get_link_form_control(link);

	if (!fc) {
		return true;
	}
	struct form_state *fs = find_form_state(doc_view, fc);

	if (!fs) {
		return true;
	}

	if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO) {
		return true;
	}
	fs->state = args[0].toBoolean();

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
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	dom_html_element *el = (dom_html_element *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
		exc = dom_html_element_set_class_name(el, classstr);
		interpreter->changed = 1;
		dom_string_unref(classstr);
		debug_dump_xhtml(document->dom);
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
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
			interpreter->changed = 1;
			dom_string_unref(dir);
			debug_dump_xhtml(document->dom);
		}
	}
	mem_free(str);

	return true;
}

static bool
element_set_property_href(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		return true;
	}
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return false;
	}
	size_t len = strlen(str);
	dom_string *hrefstr = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, len, &hrefstr);

	if (exc == DOM_NO_ERR && hrefstr) {
		exc = dom_element_set_attribute(el, corestring_dom_href, hrefstr);
		interpreter->changed = 1;
		dom_string_unref(hrefstr);
		debug_dump_xhtml(document->dom);
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
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
		interpreter->changed = 1;
		dom_string_unref(idstr);
		debug_dump_xhtml(document->dom);
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
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
	dom_node_unref(doc);
	dom_node_unref(fragment);
	dom_node_unref(child);
	dom_node_unref(html);
	dom_nodelist_unref(bodies);
	dom_node_unref(body);
	mem_free(s);
	interpreter->changed = 1;
	debug_dump_xhtml(document->dom);

	return true;
}

static bool
element_set_property_innerText(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return element_set_property_textContent(ctx, argc, vp);
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
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
		interpreter->changed = 1;
		dom_string_unref(langstr);
		debug_dump_xhtml(document->dom);
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
	args.rval().setUndefined();

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		return true;
	}
	dom_node *parent = NULL;
	dom_exception exc = dom_node_get_parent_node(el, &parent);

	if (exc != DOM_NO_ERR) {
		return true;
	}

	char *s = jsval_to_string(ctx, args[0]);

	if (!s) {
		return false;
	}
	size_t size = strlen(s);
	struct dom_node *cref = NULL;

	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	dom_hubbub_parser *parser = NULL;
	struct dom_document *doc = NULL;
	struct dom_document_fragment *fragment = NULL;
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

	/* The first child in the fragment will be an HTML element
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

	/* Migrate the children */
	exc = dom_node_get_first_child(body, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		exc = dom_node_remove_child(body, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(cref);

		dom_node *spare = NULL;
		exc = dom_node_insert_before(parent, child, el, &spare);

		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(spare);
		dom_node_unref(cref);
		dom_node_unref(child);
		child = NULL;
		exc = dom_node_get_first_child(body, &child);
		if (exc != DOM_NO_ERR) goto out;
	}
	exc = dom_node_remove_child(parent, el, &cref);

	if (exc != DOM_NO_ERR) goto out;
out:
	if (parser != NULL) {
		dom_hubbub_parser_destroy(parser);
	}
	dom_node_unref(doc);
	dom_node_unref(fragment);
	dom_node_unref(child);
	dom_node_unref(html);
	dom_nodelist_unref(bodies);
	dom_node_unref(body);
	dom_node_unref(cref);
	dom_node_unref(parent);

	mem_free(s);
	interpreter->changed = 1;
	debug_dump_xhtml(document->dom);

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
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	dom_string *titlestr = NULL;
	dom_exception exc;
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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

static bool
element_set_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct view_state *vs = interpreter->vs;

	args.rval().setUndefined();

	if (!vs) {
		return true;
	}

	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return true;
	}

	struct document *doc = doc_view->document;

	if (!doc) {
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		return true;
	}
	int offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		return true;
	}

	int linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		return true;
	}
	struct link *link = &doc->links[linknum];
	struct el_form_control *fc = get_link_form_control(link);

	if (!fc) {
		return true;
	}
	struct form_state *fs = find_form_state(doc_view, fc);

	if (!fs) {
		return true;
	}

	if (fc->type != FC_FILE) {
		mem_free_set(&fs->value, jsval_to_string(ctx, args[0]));
		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD) {
			fs->state = strlen(fs->value);
		}
	}

	return true;
}


static bool element_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_appendChild(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_blur(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_click(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_cloneNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_closest(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_contains(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_dispatchEvent(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_focus(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_getAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_getAttributeNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_getBoundingClientRect(JSContext *ctx, unsigned int argc, JS::Value *rval);
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
static bool element_removeAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_removeChild(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_replaceWith(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_setAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec element_funcs[] = {
	{ "addEventListener",	element_addEventListener,	3 },
	{ "appendChild",	element_appendChild,	1 },
	{ "blur",	element_blur,		0 },
	{ "click",	element_click,		0 },
	{ "cloneNode",	element_cloneNode,	1 },
	{ "closest",	element_closest,	1 },
	{ "contains",	element_contains,	1 },
	{ "dispatchEvent", element_dispatchEvent,	1 },
	{ "focus",	element_focus,		0 },
	{ "getAttribute",	element_getAttribute,	1 },
	{ "getAttributeNode",	element_getAttributeNode,	1 },
	{ "getBoundingClientRect",	element_getBoundingClientRect, 	0 },
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
	{ "removeAttribute",	element_removeAttribute,	1 },
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
	NODEINFO(el);

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
	struct ele_listener *l;

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
	struct ele_listener *n = (struct ele_listener *)mem_calloc(1, sizeof(*n));

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
		exc = dom_event_listener_create(element_event_handler, el_private, &el_private->listener);

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
	dom_string_unref(typ);
	dom_event_listener_unref(el_private->listener);
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
	NODEINFO(el);

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

	struct ele_listener *l;

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
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
		JSObject *obj = getNode(ctx, res);
		dom_node_unref(res);
		args.rval().setObject(*obj);
		debug_dump_xhtml(document->dom);
		return true;
	}

	return false;
}

/* @element_funcs{"blur"} */
static bool
element_blur(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	/* We are a text-mode browser and there *always* has to be something
	 * selected.  So we do nothing for now. (That was easy.) */
	return true;
}

static bool
element_click(JSContext *ctx, unsigned int argc, JS::Value *rval)
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
	args.rval().setUndefined();
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
		return true;
	}

	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return true;
	}

	struct document *doc = doc_view->document;

	if (!doc) {
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		return true;
	}
	int offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		return true;
	}

	int linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		return true;
	}
	struct session *ses = doc_view->session;

	jump_to_link_number(ses, doc_view, linknum);

	if (enter(ses, doc_view, 0) == FRAME_EVENT_REFRESH) {
		refresh_view(ses, doc_view, 0);
	} else {
		print_screen_status(ses);
	}

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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
	JSObject *obj = getNode(ctx, clone);
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
		dom_node_unref(prev_next);
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
	char *selector = jsval_to_string(ctx, args[0]);
	if (!selector) {
		args.rval().setNull();
		return true;
	}

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	void *res = NULL;

	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}
	dom_node *root = NULL; /* root element of document */
	/* Get root element */
	dom_exception exc = dom_document_get_document_element(document->dom, &root);

	if (exc != DOM_NO_ERR || !root) {
		args.rval().setNull();
		return true;
	}

	if (el) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_ref(el);
	}

	while (el) {
		res = el_match_selector(selector, el);

		if (res) {
			break;
		}
		if (el == root) {
			break;
		}
		dom_node *node = NULL;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		exc = dom_node_get_parent_node(el, &node);
		if (exc != DOM_NO_ERR || !node) {
			break;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(el);
		el = node;
	}
	mem_free(selector);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(root);

#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(el);

	if (!res) {
		args.rval().setNull();
		return true;
	}
	JSObject *ret = getNode(ctx, res);
	dom_node_unref(res);
	args.rval().setObject(*ret);

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
	NODEINFO(el);

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
element_focus(JSContext *ctx, unsigned int argc, JS::Value *rval)
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
	args.rval().setUndefined();
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
		return true;
	}

	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return true;
	}

	struct document *doc = doc_view->document;

	if (!doc) {
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		return true;
	}
	int offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		return true;
	}

	int linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		return true;
	}
	jump_to_link_number(doc_view->session, doc_view, linknum);

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
	NODEINFO(el);

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
	NODEINFO(el);

	dom_exception exc;
	dom_string *attr_name = NULL;
	dom_attr *attr = NULL;

	if (!el) {
		args.rval().setUndefined();
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
	exc = dom_element_get_attribute_node(el, attr_name, &attr);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR || !attr) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getAttr(ctx, attr);
	args.rval().setObject(*obj);

	return true;
}

static bool
element_getBoundingClientRect(JSContext *ctx, unsigned int argc, JS::Value *rval)
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
	JSObject *drect = getDomRect(ctx);

	if (!drect) {
		args.rval().setNull();
		return true;
	}
	args.rval().setObject(*drect);

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
	NODEINFO(el);

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
	NODEINFO(el);

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
	NODEINFO(el);

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
	NODEINFO(el);

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
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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

	dom_exception err;
	dom_node *spare = NULL;

	err = dom_node_insert_before(el, child, next_sibling, &spare);
	if (err != DOM_NO_ERR || !spare) {
		return false;
	}
	JSObject *obj = getNode(ctx, spare);
	dom_node_unref(spare);
	args.rval().setObject(*obj);
	interpreter->changed = 1;
	debug_dump_xhtml(document->dom);

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
	NODEINFO(el);

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
	NODEINFO(el);

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
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	char *selector = jsval_to_string(ctx, args[0]);

	if (!selector) {
		args.rval().setBoolean(false);
		return true;
	}
	void *res = el_match_selector(selector, el);
	mem_free(selector);

	args.rval().setBoolean(res != NULL);
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
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
		JSObject *el = getNode(ctx, ret);
		dom_node_unref(ret);
		args.rval().setObject(*el);
	}

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
	JS::Realm *comp = js::GetContextRealm(ctx);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		args.rval().setNull();
		return true;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	args.rval().setUndefined();
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	dom_node *parent = NULL;
	dom_exception exc;

	if (!el) {
		return true;
	}
	exc = dom_node_get_parent_node(el, &parent);

	if (exc != DOM_NO_ERR || !parent) {
		return true;
	}
	dom_node *res = NULL;
	exc = dom_node_remove_child(parent, el, &res);
	dom_node_unref(parent);

	if (exc == DOM_NO_ERR) {
		dom_node_unref(res);
		interpreter->changed = 1;
	}

	return true;
}

static bool
element_removeAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval)
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
	args.rval().setUndefined();

	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	dom_exception exc;
	dom_string *attr_name = NULL;

	if (!el) {
		return true;
	}
	size_t len;
	char *str = jsval_to_string(ctx, args[0]);

	if (!str) {
		return true;
	}
	len = strlen(str);
	exc = dom_string_create((const uint8_t *)str, len, &attr_name);
	mem_free(str);

	if (exc != DOM_NO_ERR || !attr_name) {
		return true;
	}
	exc = dom_element_remove_attribute(el, attr_name);
	dom_string_unref(attr_name);

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
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

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
		JSObject *obj = getNode(ctx, spare);
		dom_node_unref(spare);
		args.rval().setObject(*obj);
		debug_dump_xhtml(document->dom);
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

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_exception exc;
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	args.rval().setUndefined();

	if (!el) {
		return true;
	}
	dom_node *new_child = NULL;

	if (!args[0].isNull()) {
		JS::RootedObject child1(ctx, &args[0].toObject());
		new_child = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(child1, 0);
	}

	if (!new_child) {
		return true;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;


// outerHTML
	struct string buf;
	if (!init_string(&buf)) {;
		return true;
	}
	ecmascript_walk_tree(&buf, new_child, false, false);

	size_t size = buf.length;
	char *s = buf.source;
	struct dom_node *cref = NULL;

	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	dom_hubbub_parser *parser = NULL;
	struct dom_document_fragment *fragment = NULL;
	struct dom_node *child = NULL, *html = NULL, *body = NULL;
	struct dom_nodelist *bodies = NULL;

	parse_params.enc = "UTF-8";
	parse_params.fix_enc = true;
	parse_params.enable_script = false;
	parse_params.msg = NULL;
	parse_params.script = NULL;
	parse_params.ctx = NULL;
	parse_params.daf = NULL;

	struct dom_document *doc = NULL;
	dom_node *parent = NULL;

	exc = dom_node_get_owner_document(el, &doc);
	if (exc != DOM_NO_ERR) goto out;

	exc = dom_node_get_parent_node(el, &parent);

	if (exc != DOM_NO_ERR || !parent) {
		goto out;
	}

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

	/* The first child in the fragment will be an HTML element
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

	/* Migrate the children */
	exc = dom_node_get_first_child(body, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		exc = dom_node_remove_child(body, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(cref);

		dom_node *spare = NULL;
		exc = dom_node_insert_before(parent, child, el, &spare);

		if (exc != DOM_NO_ERR) goto out;
		dom_node_unref(spare);
		dom_node_unref(cref);
		dom_node_unref(child);
		child = NULL;
		exc = dom_node_get_first_child(body, &child);
		if (exc != DOM_NO_ERR) goto out;
	}
	exc = dom_node_remove_child(parent, el, &cref);

	if (exc != DOM_NO_ERR) goto out;

	interpreter->changed = 1;
	debug_dump_xhtml(document->dom);

out:
	if (parser != NULL) {
		dom_hubbub_parser_destroy(parser);
	}
	dom_node_unref(doc);
	dom_node_unref(fragment);
	dom_node_unref(child);
	dom_node_unref(html);
	dom_nodelist_unref(bodies);
	dom_node_unref(body);
	dom_node_unref(cref);
	dom_node_unref(parent);
	done_string(&buf);

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
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setUndefined();
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(el);

	if (!el) {
		return true;
	}
	char *attr;
	char *value;
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
	interpreter->changed = 1;
	debug_dump_xhtml(document->dom);

	return true;
}


JSObject *
getElement(JSContext *ctx, void *node)
{
	struct element_private *el_private = (struct element_private *)mem_calloc(1, sizeof(*el_private));

	if (!el_private) {
		return NULL;
	}
	init_list(el_private->listeners);
	el_private->ref_count = 1;
	el_private->node = (dom_node *)node;
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

	void *old_node_data = NULL;
	dom_node_set_user_data(node,
		corestring_dom___ns_key_html_content_data,
		(void *)el, js_html_document_user_data_handler,
		(void *)&old_node_data);

	dom_node_ref((dom_node *)node);

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

	struct ele_listener *l;

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

static bool
element_dispatchEvent(JSContext *ctx, unsigned int argc, JS::Value *rval)
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
	dom_node *element = JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	NODEINFO(element);

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
element_event_handler(dom_event *event, void *pw)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct element_private *el_private = (struct element_private *)pw;
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

	struct ele_listener *l, *next;

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
