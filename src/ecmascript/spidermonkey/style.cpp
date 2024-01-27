/* The SpiderMonkey style object implementation. */

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
#include "ecmascript/libdom/dom.h"
#include "ecmascript/spidermonkey/style.h"
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

static bool style_style(JSContext *ctx, unsigned int argc, JS::Value *vp, const char *property);
static bool style_set_style(JSContext *ctx, unsigned int argc, JS::Value *vp, const char *property);
static bool style_get_property_background(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_backgroundColor(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_color(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_display(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_fontStyle(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_fontWeight(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_lineStyle(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_lineStyleType(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_textAlign(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_textDecoration(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_get_property_whiteSpace(JSContext *ctx, unsigned int argc, JS::Value *vp);

static bool style_set_property_background(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_backgroundColor(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_color(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_display(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_fontStyle(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_fontWeight(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_lineStyle(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_lineStyleType(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_textAlign(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_textDecoration(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool style_set_property_whiteSpace(JSContext *ctx, unsigned int argc, JS::Value *vp);

static void style_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
}

JSClassOps style_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	style_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass style_class = {
	"style",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&style_ops
};

static JSPropertySpec style_props[] = {
	JS_PSGS("background",	style_get_property_background, style_set_property_background, JSPROP_ENUMERATE),
	JS_PSGS("backgroundColor",	style_get_property_backgroundColor, style_set_property_backgroundColor, JSPROP_ENUMERATE),
	JS_PSGS("color",		style_get_property_color, style_set_property_color, JSPROP_ENUMERATE),
	JS_PSGS("display",	style_get_property_display, style_set_property_display, JSPROP_ENUMERATE),
	JS_PSGS("fontStyle",	style_get_property_fontStyle, style_set_property_fontStyle, JSPROP_ENUMERATE),
	JS_PSGS("fontWeight",	style_get_property_fontWeight, style_set_property_fontWeight, JSPROP_ENUMERATE),
	JS_PSGS("height",	style_get_property_height, style_set_property_height, JSPROP_ENUMERATE),
	JS_PSGS("lineStyle",	style_get_property_lineStyle, style_set_property_lineStyle, JSPROP_ENUMERATE),
	JS_PSGS("lineStyleType",	style_get_property_lineStyleType, style_set_property_lineStyleType, JSPROP_ENUMERATE),
	JS_PSGS("textAlign",	style_get_property_textAlign, style_set_property_textAlign, JSPROP_ENUMERATE),
	JS_PSGS("textDecoration",style_get_property_textDecoration, style_set_property_textDecoration, JSPROP_ENUMERATE),
	JS_PSGS("whiteSpace",	style_get_property_whiteSpace, style_set_property_whiteSpace, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
style_get_property_background(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "background");
}

static bool
style_get_property_backgroundColor(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "background-color");
}

static bool
style_get_property_color(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "color");
}

static bool
style_get_property_display(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "display");
}

static bool
style_get_property_fontStyle(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "font-style");
}

static bool
style_get_property_fontWeight(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "font-weight");
}

static bool
style_get_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "height");
}

static bool
style_get_property_lineStyle(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "line-style");
}

static bool
style_get_property_lineStyleType(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "line-style-type");
}

static bool
style_get_property_textAlign(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "text-align");
}

static bool
style_get_property_textDecoration(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "text-decoration");
}

static bool
style_get_property_whiteSpace(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_style(ctx, argc, vp, "white-space");
}

static bool
style_set_property_background(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "background");
}

static bool
style_set_property_backgroundColor(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "background-color");
}

static bool
style_set_property_color(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "color");
}

static bool
style_set_property_display(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "display");
}

static bool
style_set_property_fontStyle(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "font-style");
}

static bool
style_set_property_fontWeight(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "font-weight");
}

static bool
style_set_property_height(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "height");
}

static bool
style_set_property_lineStyle(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "line-style");
}

static bool
style_set_property_lineStyleType(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "line-style-type");
}

static bool
style_set_property_textAlign(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "text-align");
}

static bool
style_set_property_textDecoration(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "text-decoration");
}

static bool
style_set_property_whiteSpace(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return style_set_style(ctx, argc, vp, "white-space");
}

const std::map<std::string, bool> good = {
	{ "background",		true },
	{ "background-color",	true },
	{ "color",		true },
	{ "display",		true },
	{ "font-style",		true },
	{ "font-weight",	true },
	{ "list-style",		true },
	{ "list-style-type",	true },
	{ "text-align",		true },
	{ "text-decoration",	true },
	{ "white-space",	true }
};

static std::string
trimString(std::string str)
{
	const std::string whiteSpaces = " \t\n\r\f\v";
	// Remove leading whitespace
	size_t first_non_space = str.find_first_not_of(whiteSpaces);
	str.erase(0, first_non_space);
	// Remove trailing whitespace
	size_t last_non_space = str.find_last_not_of(whiteSpaces);
	str.erase(last_non_space + 1);

	return str;
}

void *
set_elstyle(const char *text)
{
	if (!text || !*text) {
		return NULL;
	}
	std::stringstream str(text);
	std::string word;
	std::string param, value;
	std::map<std::string, std::string> *css = NULL;

	while (!str.eof()) {
		getline(str, word, ';');
		std::stringstream params(word);
		getline(params, param, ':');
		getline(params, value, ':');
		param = trimString(param);
		value = trimString(value);

		if (good.find(param) != good.end()) {
			if (!css) {
				css = new std::map<std::string, std::string>;
			}
			if (css) {
				(*css)[param] = value;
			}
		}
	}

	return (void *)css;
}

char *
get_elstyle(void *m)
{
	std::map<std::string, std::string> *css = static_cast<std::map<std::string, std::string> *>(m);
	std::string delimiter("");
	std::stringstream output("");
	std::map<std::string, std::string>::iterator it;

	for (it = css->begin(); it != css->end(); it++) {
		output << delimiter << it->first << ":" << it->second;
		delimiter = ";";
	}
	char *res = stracpy(output.str().c_str());
	css->clear();
	delete css;

	return res;
}

char *
get_css_value(const char *text, const char *param)
{
	void *m = set_elstyle(text);
	char *res = NULL;

	if (!m) {
		return stracpy("");
	}

	std::map<std::string, std::string> *css = static_cast<std::map<std::string, std::string> *>(m);

	if (css->find(param) != css->end()) {
		res = stracpy((*css)[param].c_str());
	} else {
		res = stracpy("");
	}
	css->clear();
	delete css;

	return res;
}

char *
set_css_value(const char *text, const char *param, const char *value)
{
	void *m = set_elstyle(text);
	std::map<std::string, std::string> *css = NULL;

	if (m) {
		css = static_cast<std::map<std::string, std::string> *>(m);

		if (good.find(param) != good.end()) {
			(*css)[param] = value;
		}
		return get_elstyle(m);
	}

	if (good.find(param) != good.end()) {
		css = new std::map<std::string, std::string>;
		if (!css) {
			return stracpy("");
		}
		(*css)[param] = value;
		return get_elstyle((void *)css);
	}
	return stracpy("");
}

static bool
style_style(JSContext *ctx, unsigned int argc, JS::Value *vp, const char *property)
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
	if (!JS_InstanceOf(ctx, hobj, &style_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_exception exc;
	dom_string *style = NULL;
	char *res = NULL;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	exc = dom_element_get_attribute(el, corestring_dom_style, &style);

	if (exc != DOM_NO_ERR) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		return true;
	}

	if (!style || !dom_string_length(style)) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));

		if (style) {
			dom_string_unref(style);
		}
		return true;
	}

	res = get_css_value(dom_string_data(style), property);
	dom_string_unref(style);

	if (!res) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, res));
	mem_free(res);

	return true;
}

static bool
style_set_style(JSContext *ctx, unsigned int argc, JS::Value *vp, const char *property)
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
	if (!JS_InstanceOf(ctx, hobj, &style_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_exception exc;
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(hobj, 0);
	dom_string *style = NULL;
	dom_string *stylestr = NULL;
	char *res = NULL;
	char *value;

	if (!el) {
		args.rval().setNull();
		return true;
	}
	value = jsval_to_string(ctx, args[0]);

	if (!value) {
		return false;
	}
	args.rval().setUndefined();
	exc = dom_element_get_attribute(el, corestring_dom_style, &style);

	if (exc != DOM_NO_ERR) {
		return true;
	}

	if (!style || !dom_string_length(style)) {
		res = set_css_value("", property, value);

		if (style) {
			dom_string_unref(style);
		}
	} else {
		res = set_css_value(dom_string_data(style), property, value);
		dom_string_unref(style);
	}
	mem_free(value);
	exc = dom_string_create((const uint8_t *)res, strlen(res), &stylestr);

	if (exc == DOM_NO_ERR && stylestr) {
		exc = dom_element_set_attribute(el, corestring_dom_style, stylestr);
		interpreter->changed = 1;
		dom_string_unref(stylestr);
	}
	mem_free(res);
	return true;
}

JSObject *
getStyle(JSContext *ctx, void *node)
{
	JSObject *el = JS_NewObject(ctx, &style_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) style_props);
//	spidermonkey_DefineFunctions(ctx, el, attributes_funcs);

	JS::SetReservedSlot(el, 0, JS::PrivateValue(node));

	return el;
}
