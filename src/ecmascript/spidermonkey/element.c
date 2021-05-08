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

#include <iostream>
#include <algorithm>
#include <string>

using namespace htmlcxx;

static bool element_get_property_childElementCount(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_className(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_className(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_dir(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_id(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_id(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_innerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_innerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_lang(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_get_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool element_set_property_outerHtml(JSContext *ctx, unsigned int argc, JS::Value *vp);
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
	JS_PSG("childElementCount",	element_get_property_childElementCount, JSPROP_ENUMERATE),
	JS_PSGS("className",	element_get_property_className, element_set_property_className, JSPROP_ENUMERATE),
	JS_PSGS("dir",	element_get_property_dir, element_set_property_dir, JSPROP_ENUMERATE),
	JS_PSGS("id",	element_get_property_id, element_set_property_id, JSPROP_ENUMERATE),
	JS_PSGS("innerHTML",	element_get_property_innerHtml, element_set_property_innerHtml, JSPROP_ENUMERATE),
	JS_PSGS("lang",	element_get_property_lang, element_set_property_lang, JSPROP_ENUMERATE),
	JS_PSGS("outerHTML",	element_get_property_outerHtml, element_set_property_outerHtml, JSPROP_ENUMERATE),
	JS_PSG("tagName",	element_get_property_tagName, JSPROP_ENUMERATE),
	JS_PSGS("textContent",	element_get_property_textContent, element_set_property_textContent, JSPROP_ENUMERATE),
	JS_PSGS("title",	element_get_property_title, element_set_property_title, JSPROP_ENUMERATE),
	JS_PS_END
};

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	tree<HTML::Node> *dom = document->dom;

	tree<HTML::Node>::iterator beg = dom->begin();
	tree<HTML::Node>::iterator end = dom->end();
	tree<HTML::Node>::iterator it;

	int res = 0;
	unsigned int offset = el->begin()->offset();

	for (it = beg; it != end; ++it) {
		if (it->offset() == offset) {
			res = it.number_of_children();
			break;
		}
	}

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	tree<HTML::Node>::iterator it = el->begin();
	it->parseAttributes();
	std::string v = it->attribute("class").second;

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	tree<HTML::Node>::iterator it = el->begin();
	it->parseAttributes();
	std::string v = it->attribute("dir").second;

	if (v != "auto" && v != "ltr" && v != "rtl") {
		v = "";
	}

	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	tree<HTML::Node>::iterator it = el->begin();
	it->parseAttributes();
	std::string v = it->attribute("id").second;

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	tree<HTML::Node>::iterator it = el->begin();
	it->parseAttributes();
	std::string v = it->attribute("lang").second;

	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	tree<HTML::Node>::iterator it = el->begin();
	std::string v = it->tagName();
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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	tree<HTML::Node>::iterator it = el->begin();
	it->parseAttributes();
	std::string v = it->attribute("title").second;

	args.rval().setString(JS_NewStringCopyZ(ctx, v.c_str()));

	return true;
}


static int was_el = 0;

static void
walk_tree(struct string *buf, tree<HTML::Node> const &dom, const unsigned int offset)
{
	int local = 0;
	tree<HTML::Node>::iterator it = dom.begin();
	if (was_el) add_to_string(buf, it->text().c_str());

	if (!was_el && it->isTag()) {
		if (it->offset() == offset) {
			was_el = 1;
			local = 1;
		}
	}

	for (tree<HTML::Node>::sibling_iterator childIt = dom.begin(it); childIt != dom.end(it); ++childIt)
	{
		walk_tree(buf, childIt, offset);
	}
	if (was_el) {
		if (!local) {
			add_to_string(buf, it->closingText().c_str());
		} else {
			was_el = 0;
		}
	}
}

static void
walk_tree_content(struct string *buf, tree<HTML::Node> const &dom, const unsigned int offset)
{
	int local = 0;
	tree<HTML::Node>::iterator it = dom.begin();
	if (was_el && !it->isTag()) add_to_string(buf, it->text().c_str());

	if (!was_el && it->isTag()) {
		if (it->offset() == offset) {
			was_el = 1;
			local = 1;
		}
	}

	for (tree<HTML::Node>::sibling_iterator childIt = dom.begin(it); childIt != dom.end(it); ++childIt)
	{
		walk_tree_content(buf, childIt, offset);
	}
	if (was_el) {
		if (!local) {
			//add_to_string(buf, it->closingText().c_str());
		} else {
			was_el = 0;
		}
	}
}


static void
walk_tree_outer(struct string *buf, tree<HTML::Node> const &dom, const unsigned int offset)
{
	int local = 0;
	tree<HTML::Node>::iterator it = dom.begin();

	if (!was_el && it->isTag()) {
		if (it->offset() == offset) {
			was_el = 1;
			local = 1;
		}
	}

	if (was_el) add_to_string(buf, it->text().c_str());

	for (tree<HTML::Node>::sibling_iterator childIt = dom.begin(it); childIt != dom.end(it); ++childIt)
	{
		walk_tree_outer(buf, childIt, offset);
	}
	if (was_el) {
		add_to_string(buf, it->closingText().c_str());

		if (local) {
			was_el = 0;
		}
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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	tree<HTML::Node> *dom = document->dom;

	struct string buf;
	init_string(&buf);
	was_el = 0;

	walk_tree(&buf, *dom, el->begin()->offset());

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	tree<HTML::Node> *dom = document->dom;

	struct string buf;
	init_string(&buf);
	was_el = 0;

	walk_tree_outer(&buf, *dom, el->begin()->offset());

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);

	if (!el) {
		args.rval().setNull();
		return true;
	}

	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	tree<HTML::Node> *dom = document->dom;

	struct string buf;
	init_string(&buf);
	was_el = 0;

	walk_tree_content(&buf, *dom, el->begin()->offset());

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

static bool element_hasAttribute(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool element_hasAttributes(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec element_funcs[] = {
	{ "hasAttribute",		element_hasAttribute,	1 },
	{ "hasAttributes",		element_hasAttributes,	0 },
	{ NULL }
};

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);
	tree<HTML::Node>::iterator it = el->begin();
	it->parseAttributes();

	std::string attr = JS_EncodeString(ctx, args[0].toString());
	args.rval().setBoolean(it->attribute(attr).first);

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

	tree<HTML::Node> *el = JS_GetPrivate(hobj);
	tree<HTML::Node>::iterator it = el->begin();
	it->parseAttributes();
	const std::map<std::string, std::string> attrs = it->attributes();
	args.rval().setBoolean(!attrs.empty());

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
