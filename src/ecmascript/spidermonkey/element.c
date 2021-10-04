/* The SpiderMonkey html element objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

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
#include "ecmascript/spidermonkey/css2xpath.h"
#include "ecmascript/spidermonkey/element.h"
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

#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>
#include <libxml++/attributenode.h>
#include <libxml++/parsers/domparser.h>

#include <iostream>
#include <algorithm>
#include <string>

static bool htmlCollection_set_items(JSContext *ctx, JS::HandleObject hobj, void *node);
static bool nodeList_set_items(JSContext *ctx, JS::HandleObject hobj, void *node);
static bool attributes_set_items(JSContext *ctx, JS::HandleObject hobj, void *node);

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

JSClassOps element_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass element_class = {
	"element",
	JSCLASS_HAS_PRIVATE,
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Element::AttributeList *attrs = new xmlpp::Element::AttributeList;

	*attrs = el->get_attributes();

	if (attrs->size() == 0) {
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	auto nodes = el->get_children();
	if (nodes.empty()) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Node::NodeSet *list = new xmlpp::Node::NodeSet;

	auto it = nodes.begin();
	auto end = nodes.end();

	for (; it != end; ++it) {
		const auto element = dynamic_cast<xmlpp::Element*>(*it);

		if (element) {
			list->push_back(reinterpret_cast<xmlpp::Node*>(element));
		}
	}

	if (list->empty()) {
		delete list;
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getCollection(ctx, list);
	args.rval().setObject(*elem);
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	int res = el->get_children().size();
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Node::NodeList *nodes = new xmlpp::Node::NodeList;

	*nodes = el->get_children();
	if (nodes->empty()) {
		delete nodes;
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getNodeList(ctx, nodes);
	args.rval().setObject(*elem);
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = el->get_attribute_value("class");
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = el->get_attribute_value("dir");

	if (v != "auto" && v != "ltr" && v != "rtl") {
		v = "";
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	auto node = el->get_first_child();

	if (!node) {
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	auto nodes = el->get_children();
	if (nodes.empty()) {
		args.rval().setNull();
		return true;
	}

	auto it = nodes.begin();
	auto end = nodes.end();

	for (; it != end; ++it) {
		const auto element = dynamic_cast<const xmlpp::Element*>(*it);

		if (element) {
			JSObject *elem = getElement(ctx, element);
			args.rval().setObject(*elem);
			return true;
		}
	}
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = el->get_attribute_value("id");
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = el->get_attribute_value("lang");
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	auto nodes = el->get_children();
	if (nodes.empty()) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getElement(ctx, *(nodes.rbegin()));
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	auto nodes = el->get_children();
	if (nodes.empty()) {
		args.rval().setNull();
		return true;
	}

	auto it = nodes.rbegin();
	auto end = nodes.rend();

	for (; it != end; ++it) {
		const auto element = dynamic_cast<const xmlpp::Element*>(*it);

		if (element) {
			JSObject *elem = getElement(ctx, element);
			args.rval().setObject(*elem);
			return true;
		}
	}
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Node *node = el;

	while (true) {
		node = node->get_next_sibling();

		if (!node) {
			args.rval().setNull();
			return true;
		}
		xmlpp::Element *next = dynamic_cast<const xmlpp::Element*>(node);

		if (next) {
			JSObject *elem = getElement(ctx, next);
			args.rval().setObject(*elem);
			return true;
		}
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Node *node = JS_GetPrivate(hobj);

	xmlpp::ustring v;

	if (!node) {
		args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));
		return true;
	}

	auto el = dynamic_cast<const xmlpp::Element*>(node);

	if (el) {
		v = el->get_name();
		std::transform(v.begin(), v.end(), v.begin(), ::toupper);
	} else {
		auto el = dynamic_cast<const xmlpp::Attribute*>(node);
		if (el) {
			v = el->get_name();
		} else if (dynamic_cast<const xmlpp::TextNode*>(node)) {
			v = "#text";
		} else if (dynamic_cast<const xmlpp::CommentNode*>(node)) {
			v = "#comment";
		}
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Node *node = JS_GetPrivate(hobj);

	if (!node) {
		args.rval().setNull();
		return true;
	}

	int ret = 8;

	if (dynamic_cast<const xmlpp::Element*>(node)) {
		ret = 1;
	} else if (dynamic_cast<const xmlpp::Attribute*>(node)) {
		ret = 2;
	} else if (dynamic_cast<const xmlpp::TextNode*>(node)) {
		ret = 3;
	} else if (dynamic_cast<const xmlpp::CommentNode*>(node)) {
		ret = 8;
	}
	args.rval().setInt32(ret);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Node *node = JS_GetPrivate(hobj);

	if (!node) {
		args.rval().setNull();
		return true;
	}

	if (dynamic_cast<const xmlpp::Element*>(node)) {
		args.rval().setNull();
		return true;
	}

	auto el = dynamic_cast<const xmlpp::Attribute*>(node);

	if (el) {
		xmlpp::ustring v = el->get_value();
		args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));
		return true;
	}

	auto el2 = dynamic_cast<const xmlpp::TextNode*>(node);

	if (el2) {
		xmlpp::ustring v = el2->get_content();
		args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));
		return true;
	}

	auto el3 = dynamic_cast<const xmlpp::CommentNode*>(node);

	if (el3) {
		xmlpp::ustring v = el3->get_content();
		args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));
		return true;
	}

	args.rval().setUndefined();
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	auto node = el->get_next_sibling();

	if (!node) {
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	auto node = dynamic_cast<xmlpp::Element*>(el->get_parent());

	if (!node) {
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	auto node = el->get_parent();

	if (!node) {
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Node *node = el;

	while (true) {
		node = node->get_previous_sibling();

		if (!node) {
			args.rval().setNull();
			return true;
		}
		xmlpp::Element *next = dynamic_cast<const xmlpp::Element*>(node);

		if (next) {
			JSObject *elem = getElement(ctx, next);
			args.rval().setObject(*elem);
			return true;
		}
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	auto node = el->get_previous_sibling();

	if (!node) {
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = el->get_name();
	std::transform(v.begin(), v.end(), v.begin(), ::toupper);
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = el->get_attribute_value("title");
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static int was_el = 0;

static void
dump_element(struct string *buf, xmlpp::Element *element, bool toSort = false)
{
	add_char_to_string(buf, '<');
	add_to_string(buf, element->get_name().c_str());
	auto attrs = element->get_attributes();
	if (toSort) {
		attrs.sort([](const xmlpp::Attribute *a1, const xmlpp::Attribute *a2)
		{
			if (a1->get_name() == a2->get_name()) {
				return a1->get_value() < a2->get_value();
			}
			return a1->get_name() < a2->get_name();
		});
	}
	auto it = attrs.begin();
	auto end = attrs.end();
	for (;it != end; ++it) {
		add_char_to_string(buf, ' ');
		add_to_string(buf, (*it)->get_name().c_str());
		add_char_to_string(buf, '=');
		add_char_to_string(buf, '"');
		add_to_string(buf, (*it)->get_value().c_str());
		add_char_to_string(buf, '"');
	}
	add_char_to_string(buf, '>');
}

void
walk_tree(struct string *buf, void *nod, bool start = true, bool toSortAttrs = false)
{
	xmlpp::Node *node = nod;

	if (!start) {
		const auto textNode = dynamic_cast<const xmlpp::ContentNode*>(node);

		if (textNode) {
			add_to_string(buf, textNode->get_content().c_str());
		} else {
			const auto element = dynamic_cast<const xmlpp::Element*>(node);

			if (element) {
				dump_element(buf, element, toSortAttrs);
			}
		}
	}

	auto childs = node->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		walk_tree(buf, *it, false, toSortAttrs);
	}

	if (!start) {
		const auto element = dynamic_cast<const xmlpp::Element*>(node);
		if (element) {
			add_to_string(buf, "</");
			add_to_string(buf, element->get_name().c_str());
			add_char_to_string(buf, '>');
		}
	}
}

static void
walk_tree_content(struct string *buf, xmlpp::Node *node)
{
	const auto nodeText = dynamic_cast<const xmlpp::TextNode*>(node);

	if (nodeText) {
		add_to_string(buf, nodeText->get_content().c_str());
	}

	auto childs = node->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		walk_tree_content(buf, *it);
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	struct string buf;
	init_string(&buf);
	walk_tree(&buf, el);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}
	struct string buf;
	init_string(&buf);
	walk_tree(&buf, el, false);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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
	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	struct string buf;
	init_string(&buf);

	walk_tree_content(&buf, el);

	args.rval().setString(JS_NewStringCopyZ(ctx, buf.source));
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);
	if (!el) {
		return true;
	}
	char *val = jsval_to_string(ctx, args[0]);

	xmlpp::ustring value = val;
	el->set_attribute("class", value);
	interpreter->changed = true;
	mem_free_if(val);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);
	if (!el) {
		return true;
	}

	char *val = jsval_to_string(ctx, args[0]);
	xmlpp::ustring value = val;

	if (value == "ltr" || value == "rtl" || value == "auto") {
		el->set_attribute("dir", value);
		interpreter->changed = true;
	}
	mem_free_if(val);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);
	if (!el) {
		return true;
	}

	char *val = jsval_to_string(ctx, args[0]);
	xmlpp::ustring value = val;
	el->set_attribute("id", value);
	interpreter->changed = true;

	mem_free_if(val);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);
	if (!el) {
		return true;
	}

	auto children = el->get_children();
	auto it = children.begin();
	auto end = children.end();
	for (;it != end; ++it) {
		xmlpp::Node::remove_node(*it);
	}

	xmlpp::ustring text = "<root>";
	char *vv = jsval_to_string(ctx, args[0]);
	text += vv;
	text += "</root>";
	mem_free_if(vv);

	xmlDoc* doc = htmlReadDoc((xmlChar*)text.c_str(), NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	// Encapsulate raw libxml document in a libxml++ wrapper
	xmlpp::Document doc1(doc);

	auto root = doc1.get_root_node();
	auto root1 = root->find("//root")[0];
	auto children2 = root1->get_children();
	auto it2 = children2.begin();
	auto end2 = children2.end();
	for (; it2 != end2; ++it2) {
		el->import_node(*it2);
	}
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);
	if (!el) {
		return true;
	}

	char *val = jsval_to_string(ctx, args[0]);
	xmlpp::ustring value = val;
	el->set_attribute("lang", value);
	interpreter->changed = true;

	mem_free_if(val);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);
	if (!el) {
		return true;
	}

	char *val = jsval_to_string(ctx, args[0]);
	xmlpp::ustring value = val;
	el->set_attribute("title", value);
	interpreter->changed = true;

	mem_free_if(val);

	return true;
}

static bool element_appendChild(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_cloneNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_contains(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_getAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_getAttributeNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasAttributes(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasChildNodes(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_insertBefore(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_isEqualNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_isSameNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_querySelector(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_querySelectorAll(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_remove(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_removeChild(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_setAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec element_funcs[] = {
	{ "appendChild",	element_appendChild,	1 },
	{ "cloneNode",	element_cloneNode,	1 },
	{ "contains",	element_contains,	1 },
	{ "getAttribute",	element_getAttribute,	1 },
	{ "getAttributeNode",	element_getAttributeNode,	1 },
	{ "hasAttribute",		element_hasAttribute,	1 },
	{ "hasAttributes",		element_hasAttributes,	0 },
	{ "hasChildNodes",		element_hasChildNodes,	0 },
	{ "insertBefore",		element_insertBefore,	2 },
	{ "isEqualNode",		element_isEqualNode,	1 },
	{ "isSameNode",			element_isSameNode,	1 },
	{ "querySelector",		element_querySelector,	1 },
	{ "querySelectorAll",		element_querySelectorAll,	1 },
	{ "remove",		element_remove,	0 },
	{ "removeChild",	element_removeChild,	1 },
	{ "setAttribute",	element_setAttribute,	2 },
	{ NULL }
};

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

static void
check_contains(xmlpp::Node *node, xmlpp::Node *searched, bool *result_set, bool *result)
{
	if (*result_set) {
		return;
	}

	auto childs = node->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		if (*it == searched) {
			*result_set = true;
			*result = true;
			return;
		}
		check_contains(*it, searched, result_set, result);
	}
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	JS::RootedObject node(ctx, &args[0].toObject());
	xmlpp::Node *el2 = JS_GetPrivate(node);
	el->import_node(el2);
	interpreter->changed = true;

	JSObject *obj = getElement(ctx, el2);
	if (obj) {
		args.rval().setObject(*obj);
	} else {
		args.rval().setNull();
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	xmlpp::Document *doc2 = document->dom;
	xmlDoc *docu = doc2->cobj();
	xmlNode *xmlnode = xmlNewDocFragment(docu);

	if (!xmlnode) {
		args.rval().setNull();
		return true;
	}

	xmlpp::Node *node = new xmlpp::Node(xmlnode);

	try {
		xmlpp::Node *node2 = node->import_node(el, args[0].toBoolean());
		if (!node2) {
			args.rval().setNull();
			return true;
		}
		JSObject *obj = getElement(ctx, node2);
		if (!obj) {
			args.rval().setNull();
			return true;
		}
		args.rval().setObject(*obj);
		return true;
	} catch (xmlpp::exception e) {
		args.rval().setNull();
		return true;
	}
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::RootedObject node(ctx, &args[0].toObject());

	xmlpp::Element *el2 = JS_GetPrivate(node);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	char *vv = jsval_to_string(ctx, args[0]);

	if (vv) {
		xmlpp::ustring v = vv;
		xmlpp::Attribute *attr = el->get_attribute(v);

		if (!attr) {
			args.rval().setNull();
		} else {
			xmlpp::ustring val = attr->get_value();
			args.rval().setString(JS_NewStringCopyZ(ctx, val.c_str()));
		}
		mem_free(vv);
	} else {
		args.rval().setNull();
	}

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setUndefined();
		return true;
	}
	char *vv = jsval_to_string(ctx, args[0]);
	xmlpp::ustring v = vv;
	xmlpp::Attribute *attr = el->get_attribute(v);
	JSObject *obj = getAttr(ctx, attr);
	args.rval().setObject(*obj);

	mem_free_if(vv);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	char *vv = jsval_to_string(ctx, args[0]);
	xmlpp::ustring v = vv;
	xmlpp::Attribute *attr = el->get_attribute(v);
	args.rval().setBoolean((bool)attr);

	mem_free_if(vv);

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
	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	auto attrs = el->get_attributes();
	args.rval().setBoolean((bool)attrs.size());

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
	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	auto children = el->get_children();
	args.rval().setBoolean((bool)children.size());

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
		args.rval().setBoolean(false);
		return true;
	}
	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		return true;
	}

	JS::RootedObject next_sibling1(ctx, &args[1].toObject());
	JS::RootedObject child1(ctx, &args[0].toObject());

	xmlpp::Node *next_sibling = JS_GetPrivate(next_sibling1);

	if (!next_sibling) {
		return nullptr;
	}

	xmlpp::Node *child = JS_GetPrivate(child1);
	auto node = xmlAddPrevSibling(next_sibling->cobj(), child->cobj());
	auto res = el_add_child_element_common(child->cobj(), node);

	JSObject *elem = getElement(ctx, res);
	args.rval().setObject(*elem);
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::RootedObject node(ctx, &args[0].toObject());

	xmlpp::Element *el2 = JS_GetPrivate(node);

	struct string first;
	struct string second;

	init_string(&first);
	init_string(&second);

	walk_tree(&first, el, false, true);
	walk_tree(&second, el2, false, true);

	args.rval().setBoolean(!strcmp(first.source, second.source));

	done_string(&first);
	done_string(&second);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}

	JS::RootedObject node(ctx, &args[0].toObject());

	xmlpp::Element *el2 = JS_GetPrivate(node);
	args.rval().setBoolean(el == el2);

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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}

	struct string cssstr;
	init_string(&cssstr);
	jshandle_value_to_char_string(&cssstr, ctx, args[0]);
	xmlpp::ustring css = cssstr.source;

	xmlpp::ustring xpath = css2xpath(css);

	done_string(&cssstr);

	auto elements = el->find(xpath);

	if (elements.size() == 0) {
		args.rval().setNull();
		return true;
	}

	auto node = elements[0];

	JSObject *elem = getElement(ctx, node);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
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

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}

	struct string cssstr;

	init_string(&cssstr);
	jshandle_value_to_char_string(&cssstr, ctx, args[0]);
	xmlpp::ustring css = cssstr.source;

	xmlpp::ustring xpath = css2xpath(css);

	done_string(&cssstr);

	xmlpp::Node::NodeSet *elements = new xmlpp::Node::NodeSet;

	*elements = el->find(xpath);

	if (elements->size() == 0) {
		args.rval().setNull();
		return true;
	}

	JSObject *elem = getCollection(ctx, elements);

	if (elem) {
		args.rval().setObject(*elem);
	} else {
		args.rval().setNull();
	}

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		return true;
	}

	xmlpp::Node::remove_node(el);
	interpreter->changed = true;

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el || !args[0].isObject()) {
		args.rval().setNull();
		return true;
	}

	JS::RootedObject node(ctx, &args[0].toObject());

	auto children = el->get_children();
	auto it = children.begin();
	auto end = children.end();
	xmlpp::Element *el2 = JS_GetPrivate(node);

	for (;it != end; ++it) {
		if (*it == el2) {
			xmlpp::Node::remove_node(el2);
			interpreter->changed = true;
			JSObject *obj = getElement(ctx, el2);
			if (obj) {
				args.rval().setObject(*obj);
				return true;
			}
			break;
		}
	}
	args.rval().setNull();

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		return true;
	}

	if (args[0].isString() && args[1].isString()) {
		char *attr_c = jsval_to_string(ctx, args[0]);
		xmlpp::ustring attr = attr_c;
		char *value_c = jsval_to_string(ctx, args[1]);
		xmlpp::ustring value = value_c;
		el->set_attribute(attr, value);
		interpreter->changed = true;
		mem_free_if(attr_c);
		mem_free_if(value_c);
	}

	return true;
}

JSObject *
getElement(JSContext *ctx, void *node)
{
	JSObject *el = JS_NewObject(ctx, &element_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) element_props);
	spidermonkey_DefineFunctions(ctx, el, element_funcs);

	JS_SetPrivate(el, node);

	return el;
}

static bool htmlCollection_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool htmlCollection_namedItem(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool htmlCollection_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool htmlCollection_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);
static bool htmlCollection_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp);

JSClassOps htmlCollection_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass htmlCollection_class = {
	"htmlCollection",
	JSCLASS_HAS_PRIVATE,
	&htmlCollection_ops
};

static const spidermonkeyFunctionSpec htmlCollection_funcs[] = {
	{ "item",		htmlCollection_item,		1 },
	{ "namedItem",		htmlCollection_namedItem,	1 },
	{ NULL }
};

static bool htmlCollection_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

static JSPropertySpec htmlCollection_props[] = {
	JS_PSG("length",	htmlCollection_get_property_length, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
htmlCollection_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL)) {
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

	xmlpp::Node::NodeSet *ns = JS_GetPrivate(hobj);

	if (!ns) {
		args.rval().setInt32(0);
		return true;
	}

	args.rval().setInt32(ns->size());

	return true;
}

static bool
htmlCollection_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	int index = args[0].toInt32();
	bool ret = htmlCollection_item2(ctx, hobj, index, &rval);
	args.rval().set(rval);

	return ret;
}

static bool
htmlCollection_namedItem(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	char *str = jsval_to_string(ctx, args[0]);
	rval.setNull();
	bool ret = htmlCollection_namedItem2(ctx, hobj, str, &rval);
	args.rval().set(rval);

	mem_free_if(str);

	return ret;
}

static bool
htmlCollection_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	hvp.setUndefined();

	xmlpp::Node::NodeSet *ns = JS_GetPrivate(hobj);

	if (!ns) {
		return true;
	}

	xmlpp::Element *element;

	try {
		element = ns->at(index);
	} catch (std::out_of_range e) { return true;}

	if (!element) {
		return true;
	}

	JSObject *obj = getElement(ctx, element);
	hvp.setObject(*obj);

	return true;
}

static bool
htmlCollection_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Node::NodeSet *ns = JS_GetPrivate(hobj);

	if (!ns) {
		return true;
	}

	xmlpp::ustring name = str;

	auto it = ns->begin();
	auto end = ns->end();

	for (; it != end; ++it) {
		const auto element = dynamic_cast<const xmlpp::Element*>(*it);

		if (!element) {
			continue;
		}

		if (name == element->get_attribute_value("id")
		|| name == element->get_attribute_value("name")) {
			JSObject *obj = getElement(ctx, element);
			hvp.setObject(*obj);
			return true;
		}
	}

	return true;
}

static bool
htmlCollection_set_items(JSContext *ctx, JS::HandleObject hobj, void *node)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	int counter = 0;

	xmlpp::Node::NodeSet *ns = JS_GetPrivate(hobj);

	if (!ns) {
		return true;
	}

	xmlpp::Element *element;

	while (1) {
		try {
			element = ns->at(counter);
		} catch (std::out_of_range e) { return true;}

		if (!element) {
			return true;
		}

		JSObject *obj = getElement(ctx, element);

		if (!obj) {
			continue;
		}
		JS::RootedObject v(ctx, obj);
		JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));
		JS_SetElement(ctx, hobj, counter, ro);

		xmlpp::ustring name = element->get_attribute_value("id");
		if (name == "") {
			name = element->get_attribute_value("name");
		}
		if (name != "" && name != "item" && name != "namedItem") {
			JS_DefineProperty(ctx, hobj, name.c_str(), ro, JSPROP_ENUMERATE);
		}
		counter++;
	}

	return true;
}

static bool
htmlCollection_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	jsid id = hid.get();
	struct view_state *vs;
	JS::Value idval;

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (JSID_IS_INT(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		int index = r_idval.toInt32();
		return htmlCollection_item2(ctx, hobj, index, hvp);
	}

	if (JSID_IS_STRING(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		char *string = jsval_to_string(ctx, r_idval);

		if (string) {
			xmlpp::ustring test = string;

			if (test != "item" && test != "namedItem") {
				bool ret = htmlCollection_namedItem2(ctx, hobj, string, hvp);
				mem_free(string);
				return ret;
			}
			mem_free(string);
		}
	}

	return true;
}

JSObject *
getCollection(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *el = JS_NewObject(ctx, &htmlCollection_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) htmlCollection_props);
	spidermonkey_DefineFunctions(ctx, el, htmlCollection_funcs);

	JS_SetPrivate(el, node);
	htmlCollection_set_items(ctx, r_el, node);

	return el;
}


static bool nodeList_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool nodeList_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool nodeList_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);

JSClassOps nodeList_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass nodeList_class = {
	"nodeList",
	JSCLASS_HAS_PRIVATE,
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

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

	xmlpp::Node::NodeList *nl = JS_GetPrivate(hobj);

	if (!nl) {
		args.rval().setInt32(0);
		return true;
	}
	args.rval().setInt32(nl->size());

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
nodeList_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &nodeList_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	hvp.setUndefined();

	xmlpp::Node::NodeList *nl = JS_GetPrivate(hobj);

	if (!nl) {
		return true;
	}

	xmlpp::Element *element = nullptr;

	auto it = nl->begin();
	auto end = nl->end();
	for (int i = 0; it != end; ++it, ++i) {
		if (i == index) {
			element = *it;
			break;
		}
	}

	if (!element) {
		return true;
	}

	JSObject *obj = getElement(ctx, element);
	hvp.setObject(*obj);

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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &nodeList_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	xmlpp::Node::NodeList *nl = JS_GetPrivate(hobj);

	if (!nl) {
		return true;
	}

	auto it = nl->begin();
	auto end = nl->end();
	for (int i = 0; it != end; ++it, ++i) {
		xmlpp::Element *element = *it;

		if (element) {
			JSObject *obj = getElement(ctx, element);

			if (!obj) {
				continue;
			}

			JS::RootedObject v(ctx, obj);
			JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));
			JS_SetElement(ctx, hobj, i, ro);
		}
	}

	return true;
}

static bool
nodeList_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	jsid id = hid.get();
	struct view_state *vs;
	JS::Value idval;

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &nodeList_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (JSID_IS_INT(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		int index = r_idval.toInt32();
		return nodeList_item2(ctx, hobj, index, hvp);
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

	JS_SetPrivate(el, node);
	nodeList_set_items(ctx, r_el, node);

	return el;
}

static bool attributes_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool attributes_getNamedItem(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool attributes_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool attributes_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);
static bool attributes_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp);

JSClassOps attributes_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass attributes_class = {
	"attributes",
	JSCLASS_HAS_PRIVATE,
	&attributes_ops
};

static const spidermonkeyFunctionSpec attributes_funcs[] = {
	{ "item",		attributes_item,		1 },
	{ "getNamedItem",		attributes_getNamedItem,	1 },
	{ NULL }
};

static bool attributes_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

static JSPropertySpec attributes_props[] = {
	JS_PSG("length",	attributes_get_property_length, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
attributes_set_items(JSContext *ctx, JS::HandleObject hobj, void *node)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	int counter = 0;

	xmlpp::Element::AttributeList *al = JS_GetPrivate(hobj);

	if (!al) {
		return true;
	}

	auto it = al->begin();
	auto end = al->end();
	int i = 0;

	for (;it != end; ++it, ++i) {
		xmlpp::Attribute *attr = *it;

		if (!attr) {
			continue;
		}

		JSObject *obj = getAttr(ctx, attr);

		if (!obj) {
			continue;
		}
		JS::RootedObject v(ctx, obj);
		JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));
		JS_SetElement(ctx, hobj, i, ro);

		xmlpp::ustring name = attr->get_name();

		if (name != "" && name != "item" && name != "namedItem") {
			JS_DefineProperty(ctx, hobj, name.c_str(), ro, JSPROP_ENUMERATE);
		}
	}

	return true;
}


static bool
attributes_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
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

	xmlpp::Element::AttributeList *al = JS_GetPrivate(hobj);

	if (!al) {
		args.rval().setInt32(0);
		return true;
	}

	args.rval().setInt32(al->size());

	return true;
}

static bool
attributes_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	int index = args[0].toInt32();
	bool ret = attributes_item2(ctx, hobj, index, &rval);
	args.rval().set(rval);

	return ret;
}

static bool
attributes_getNamedItem(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	char *str = jsval_to_string(ctx, args[0]);
	bool ret = attributes_namedItem2(ctx, hobj, str, &rval);
	args.rval().set(rval);

	mem_free_if(str);

	return ret;
}

static bool
attributes_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	hvp.setUndefined();

	xmlpp::Element::AttributeList *al = JS_GetPrivate(hobj);

	if (!al) {
		return true;
	}

	auto it = al->begin();
	auto end = al->end();
	int i = 0;

	for (;it != end; ++it, ++i) {
		if (i != index) {
			continue;
		}
		xmlpp::Attribute *attr = *it;
		JSObject *obj = getAttr(ctx, attr);
		hvp.setObject(*obj);
		break;
	}

	return true;
}

static bool
attributes_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	xmlpp::Element::AttributeList *al = JS_GetPrivate(hobj);

	hvp.setUndefined();

	if (!al) {
		return true;
	}

	xmlpp::ustring name = str;

	auto it = al->begin();
	auto end = al->end();

	for (; it != end; ++it) {
		const auto attr = dynamic_cast<const xmlpp::AttributeNode*>(*it);

		if (!attr) {
			continue;
		}

		if (name == attr->get_name()) {
			JSObject *obj = getAttr(ctx, attr);
			hvp.setObject(*obj);
			return true;
		}
	}

	return true;
}

static bool
attributes_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	jsid id = hid.get();
	struct view_state *vs;
	JS::Value idval;

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (JSID_IS_INT(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		int index = r_idval.toInt32();
		return attributes_item2(ctx, hobj, index, hvp);
	}

#if 0
	if (JSID_IS_STRING(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		char *string = jsval_to_string(ctx, r_idval);

		return attributes_namedItem2(ctx, hobj, string, hvp);
	}
#endif

	return true;
}

JSObject *
getAttributes(JSContext *ctx, void *node)
{
	JSObject *el = JS_NewObject(ctx, &attributes_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) attributes_props);
	spidermonkey_DefineFunctions(ctx, el, attributes_funcs);

	JS_SetPrivate(el, node);
	attributes_set_items(ctx, r_el, node);

	return el;
}

static bool attr_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool attr_get_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp);

JSClassOps attr_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass attr_class = {
	"attr",
	JSCLASS_HAS_PRIVATE,
	&attr_ops
};

static JSPropertySpec attr_props[] = {
	JS_PSG("name",	attr_get_property_name, JSPROP_ENUMERATE),
	JS_PSG("value",	attr_get_property_value, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
attr_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attr_class, NULL)) {
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

	xmlpp::AttributeNode *attr = JS_GetPrivate(hobj);

	if (!attr) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = attr->get_name();
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static bool
attr_get_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp)
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

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attr_class, NULL)) {
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

	xmlpp::AttributeNode *attr = JS_GetPrivate(hobj);

	if (!attr) {
		args.rval().setNull();
		return true;
	}

	xmlpp::ustring v = attr->get_value();
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

JSObject *
getAttr(JSContext *ctx, void *node)
{
	JSObject *el = JS_NewObject(ctx, &attr_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) attr_props);
//	spidermonkey_DefineFunctions(ctx, el, attributes_funcs);

	JS_SetPrivate(el, node);

	return el;
}
