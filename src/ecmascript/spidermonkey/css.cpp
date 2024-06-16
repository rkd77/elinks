/* The SpiderMonkey html CSSStyleDeclaration object implementation. */

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
#include "ecmascript/spidermonkey/css.h"
#include "ecmascript/spidermonkey/element.h"
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


static bool CSSStyleDeclaration_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool CSSStyleDeclaration_getPropertyValue(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool CSSStyleDeclaration_namedItem(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool CSSStyleDeclaration_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);
static bool CSSStyleDeclaration_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp);

static void CSSStyleDeclaration_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
}

JSClassOps CSSStyleDeclaration_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	CSSStyleDeclaration_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass CSSStyleDeclaration_class = {
	"CSSStyleDeclaration",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&CSSStyleDeclaration_ops
};

static const spidermonkeyFunctionSpec CSSStyleDeclaration_funcs[] = {
	{ "getPropertyValue",	CSSStyleDeclaration_getPropertyValue,		1 },
	{ "item",		CSSStyleDeclaration_item,		1 },
	{ "namedItem",		CSSStyleDeclaration_namedItem,	1 },
	{ NULL }
};

static bool CSSStyleDeclaration_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

static JSPropertySpec CSSStyleDeclaration_props[] = {
	JS_PSG("length",	CSSStyleDeclaration_get_property_length, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
CSSStyleDeclaration_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	args.rval().setInt32(3); // fake

	return true;
}

static bool
CSSStyleDeclaration_getPropertyValue(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setUndefined();

	return true;
}

static bool
CSSStyleDeclaration_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	int index = args[0].toInt32();
	bool ret = CSSStyleDeclaration_item2(ctx, hobj, index, &rval);
	args.rval().set(rval);

	return ret;
}


static bool
CSSStyleDeclaration_namedItem(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	bool ret = CSSStyleDeclaration_namedItem2(ctx, hobj, str, &rval);
	args.rval().set(rval);

	mem_free_if(str);

	return ret;
}

static bool
CSSStyleDeclaration_item2(JSContext *ctx, JS::HandleObject hobj, int idx, JS::MutableHandleValue hvp)
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

	if (!JS_InstanceOf(ctx, hobj, &CSSStyleDeclaration_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	hvp.setString(JS_NewStringCopyZ(ctx, "0")); // fake

	return true;
}

static bool
CSSStyleDeclaration_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp)
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

	if (!JS_InstanceOf(ctx, hobj, &CSSStyleDeclaration_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	hvp.setString(JS_NewStringCopyZ(ctx, "0")); // fake

	return true;
}

static bool
CSSStyleDeclaration_set_items(JSContext *ctx, JS::HandleObject hobj, void *node)
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
	if (!JS_InstanceOf(ctx, hobj, &CSSStyleDeclaration_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::RootedString zero(ctx, JS_NewStringCopyZ(ctx, "0"));
	JS_DefineProperty(ctx, hobj, "marginTop", zero, JSPROP_ENUMERATE);
	JS_DefineProperty(ctx, hobj, "marginLeft", zero, JSPROP_ENUMERATE);
	JS_DefineProperty(ctx, hobj, "marginRight", zero, JSPROP_ENUMERATE);

	return true;
}

JSObject *
getCSSStyleDeclaration(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *el = JS_NewObject(ctx, &CSSStyleDeclaration_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) CSSStyleDeclaration_props);
	spidermonkey_DefineFunctions(ctx, el, CSSStyleDeclaration_funcs);

	JS::SetReservedSlot(el, 0, JS::PrivateValue(node));
	CSSStyleDeclaration_set_items(ctx, r_el, node);

	return el;
}
