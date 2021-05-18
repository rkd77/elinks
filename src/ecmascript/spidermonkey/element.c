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
#include <htmlcxx/html/ParserDom.h>

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
#include "ecmascript/spidermonkey/element.h"
#include "ecmascript/spidermonkey/window.h"
#include "intl/gettext/libintl.h"
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

#include <libxml++/libxml++.h>
#include <libxml++/attributenode.h>

#include <iostream>
#include <algorithm>
#include <string>

static bool element_get_property_attributes(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_childElementCount(JSContext *ctx, unsigned int argc, JS::Value *vp);
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
static bool element_get_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_lastChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_lastElementChild(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nextElementSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nextSibling(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nodeName(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_nodeType(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
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
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
};

JSClass element_class = {
	"element",
	JSCLASS_HAS_PRIVATE,
	&element_ops
};

JSPropertySpec element_props[] = {
	JS_PSG("attributes",	element_get_property_attributes, JSPROP_ENUMERATE),
	JS_PSG("childElementCount",	element_get_property_childElementCount, JSPROP_ENUMERATE),
	JS_PSGS("className",	element_get_property_className, element_set_property_className, JSPROP_ENUMERATE),
	JS_PSGS("dir",	element_get_property_dir, element_set_property_dir, JSPROP_ENUMERATE),
	JS_PSG("firstChild",	element_get_property_firstChild, JSPROP_ENUMERATE),
	JS_PSG("firstElementChild",	element_get_property_firstElementChild, JSPROP_ENUMERATE),
	JS_PSGS("id",	element_get_property_id, element_set_property_id, JSPROP_ENUMERATE),
	JS_PSGS("innerHTML",	element_get_property_innerHtml, element_set_property_innerHtml, JSPROP_ENUMERATE),
	JS_PSGS("lang",	element_get_property_lang, element_set_property_lang, JSPROP_ENUMERATE),
	JS_PSG("lastChild",	element_get_property_lastChild, JSPROP_ENUMERATE),
	JS_PSG("lastElementChild",	element_get_property_lastElementChild, JSPROP_ENUMERATE),
	JS_PSG("nextElementSibling",	element_get_property_nextElementSibling, JSPROP_ENUMERATE),
	JS_PSG("nextSibling",	element_get_property_nextSibling, JSPROP_ENUMERATE),
	JS_PSG("nodeName",	element_get_property_nodeName, JSPROP_ENUMERATE),
	JS_PSG("nodeType",	element_get_property_nodeType, JSPROP_ENUMERATE),
	JS_PSGS("outerHTML",	element_get_property_outerHtml, element_set_property_outerHtml, JSPROP_ENUMERATE),
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
element_get_property_childElementCount(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
element_get_property_className(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	std::string v = el->get_attribute_value("class");
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}


static bool
element_get_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	std::string v = el->get_attribute_value("dir");

	if (v != "auto" && v != "ltr" && v != "rtl") {
		v = "";
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static bool
element_get_property_firstChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	std::string v = el->get_attribute_value("id");
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static bool
element_get_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	std::string v = el->get_attribute_value("lang");
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static bool
element_get_property_lastChild(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	xmlpp::Node *node = JS_GetPrivate(hobj);

	if (!node) {
		args.rval().setNull();
		return true;
	}

	std::string v;
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
element_get_property_nextSibling(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
element_get_property_parentElement(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	std::string v = el->get_name();
	std::transform(v.begin(), v.end(), v.begin(), ::toupper);
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static bool
element_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	std::string v = el->get_attribute_value("title");
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static int was_el = 0;

static void
dump_element(struct string *buf, xmlpp::Element *element)
{
	add_char_to_string(buf, '<');
	add_to_string(buf, element->get_name().c_str());
	auto attrs = element->get_attributes();
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

static void
walk_tree(struct string *buf, xmlpp::Node *node, bool start = true)
{
	if (!start) {
		const auto textNode = dynamic_cast<const xmlpp::ContentNode*>(node);

		if (textNode) {
			add_to_string(buf, textNode->get_content().c_str());
		} else {
			const auto element = dynamic_cast<const xmlpp::Element*>(node);

			if (element) {
				dump_element(buf, element);
			}
		}
	}

	auto childs = node->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		walk_tree(buf, *it, false);
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;
	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	tree<HTML::Node> *dom = document->dom;

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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}

static bool
element_set_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}


static bool
element_set_property_id(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}

static bool
element_set_property_innerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}

static bool
element_set_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}


static bool
element_set_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}

static bool
element_set_property_textContent(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}


static bool
element_set_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return true;
	}

	return true;
}

static bool element_contains(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_getAttributeNode(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasAttributes(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasChildNodes(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_isSameNode(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec element_funcs[] = {
	{ "contains",	element_contains,	1 },
	{ "getAttributeNode",	element_getAttributeNode,	1 },
	{ "hasAttribute",		element_hasAttribute,	1 },
	{ "hasAttributes",		element_hasAttributes,	0 },
	{ "hasChildNodes",		element_hasChildNodes,	0 },
	{ "isSameNode",			element_isSameNode,	1 },
	{ NULL }
};

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
element_contains(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp || argc != 1) {
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

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
element_getAttributeNode(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp || argc != 1) {
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setUndefined();
		return true;
	}
	std::string v = JS_EncodeString(ctx, args[0].toString());
	xmlpp::Attribute *attr = el->get_attribute(v);
	JSObject *obj = getAttr(ctx, attr);
	args.rval().setObject(*obj);

	return true;
}


static bool
element_hasAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp || argc != 1) {
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

	xmlpp::Element *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setBoolean(false);
		return true;
	}
	std::string v = JS_EncodeString(ctx, args[0].toString());
	xmlpp::Attribute *attr = el->get_attribute(v);
	args.rval().setBoolean((bool)attr);

	return true;
}

static bool
element_hasAttributes(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp || argc != 0) {
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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp || argc != 0) {
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
element_isSameNode(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp || argc != 1) {
		return false;
	}

	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &element_class, NULL))
		return false;

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
	JS_PropertyStub, nullptr,
	htmlCollection_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	char *str = JS_EncodeString(ctx, args[0].toString());
	bool ret = htmlCollection_namedItem2(ctx, hobj, str, &rval);
	args.rval().set(rval);

	return ret;
}

static bool
htmlCollection_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL)) return false;

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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL))
		return false;

	xmlpp::Node::NodeSet *ns = JS_GetPrivate(hobj);

	hvp.setUndefined();

	if (!ns) {
		return true;
	}

	std::string name = str;

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
htmlCollection_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();
	struct view_state *vs;
	JS::Value idval;

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL)) {
		return false;
	}

	if (JSID_IS_INT(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		int index = r_idval.toInt32();
		return htmlCollection_item2(ctx, hobj, index, hvp);
	}

#if 0
	if (JSID_IS_STRING(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		char *string = JS_EncodeString(ctx, r_idval.toString());

		return htmlCollection_namedItem2(ctx, hobj, string, hvp);
	}
#endif

	return JS_PropertyStub(ctx, hobj, hid, hvp);
}

JSObject *
getCollection(JSContext *ctx, void *node)
{
	JSObject *el = JS_NewObject(ctx, &htmlCollection_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) htmlCollection_props);
	spidermonkey_DefineFunctions(ctx, el, htmlCollection_funcs);

	JS_SetPrivate(el, node);

	return el;
}

static bool attributes_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool attributes_getNamedItem(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool attributes_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool attributes_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);
static bool attributes_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp);

JSClassOps attributes_ops = {
	JS_PropertyStub, nullptr,
	attributes_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
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
attributes_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	char *str = JS_EncodeString(ctx, args[0].toString());
	bool ret = attributes_namedItem2(ctx, hobj, str, &rval);
	args.rval().set(rval);

	return ret;
}

static bool
attributes_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) return false;

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
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL))
		return false;

	xmlpp::Element::AttributeList *al = JS_GetPrivate(hobj);

	hvp.setUndefined();

	if (!al) {
		return true;
	}

	std::string name = str;

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
	jsid id = hid.get();
	struct view_state *vs;
	JS::Value idval;

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
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
		char *string = JS_EncodeString(ctx, r_idval.toString());

		return attributes_namedItem2(ctx, hobj, string, hvp);
	}
#endif

	return JS_PropertyStub(ctx, hobj, hid, hvp);
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

	return el;
}

static bool attr_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool attr_get_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp);

JSClassOps attr_ops = {
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attr_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	xmlpp::AttributeNode *attr = JS_GetPrivate(hobj);

	if (!attr) {
		args.rval().setNull();
		return true;
	}

	std::string v = attr->get_name();
	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}

static bool
attr_get_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attr_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}

	xmlpp::AttributeNode *attr = JS_GetPrivate(hobj);

	if (!attr) {
		args.rval().setNull();
		return true;
	}

	std::string v = attr->get_value();
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
