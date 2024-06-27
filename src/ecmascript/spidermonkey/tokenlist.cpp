/* The SpiderMonkey html DOM tokenlist object implementation. */

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
#include "ecmascript/spidermonkey/element.h"
#include "ecmascript/spidermonkey/tokenlist.h"
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

static bool tokenlist_add(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool tokenlist_contains(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool tokenlist_remove(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool tokenlist_toggle(JSContext *ctx, unsigned int argc, JS::Value *vp);

static void tokenlist_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_tokenlist *tl = JS::GetMaybePtrFromReservedSlot<dom_tokenlist>(obj, 0);

	if (tl) {
		dom_tokenlist_unref(tl);
	}
}

JSClassOps tokenlist_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	tokenlist_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass tokenlist_class = {
	"tokenlist",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&tokenlist_ops
};

static const spidermonkeyFunctionSpec tokenlist_funcs[] = {
	{ "add",		tokenlist_add,		1 },
	{ "contains",	tokenlist_contains,	1 },
	{ "remove",		tokenlist_remove,	1 },
	{ "toggle",		tokenlist_toggle,	1 },
	{ NULL }
};

static JSPropertySpec tokenlist_props[] = {
	JS_PS_END
};

static bool
tokenlist_add(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_tokenlist *tl = JS::GetMaybePtrFromReservedSlot<dom_tokenlist>(hobj, 0);

	if (!tl || argc < 1) {
		args.rval().setUndefined();
		return true;
	}
	char *klass = jsval_to_string(ctx, args[0]);

	if (!klass) {
		args.rval().setUndefined();
		return true;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create(klass, strlen(klass), &kl);
	mem_free(klass);

	if (exc != DOM_NO_ERR || !kl) {
		args.rval().setUndefined();
		return true;
	}
	exc = dom_tokenlist_add(tl, kl);
	dom_string_unref(kl);

	if (exc == DOM_NO_ERR) {
		interpreter->changed = true;
	}
	args.rval().setUndefined();
	return true;
}

static bool
tokenlist_contains(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_tokenlist *tl = JS::GetMaybePtrFromReservedSlot<dom_tokenlist>(hobj, 0);

	if (!tl || argc < 1) {
		args.rval().setUndefined();
		return true;
	}
	char *klass = jsval_to_string(ctx, args[0]);

	if (!klass) {
		args.rval().setUndefined();
		return true;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create(klass, strlen(klass), &kl);
	mem_free(klass);

	if (exc != DOM_NO_ERR || !kl) {
		args.rval().setUndefined();
		return true;
	}
	bool res = false;
	exc = dom_tokenlist_contains(tl, kl, &res);
	dom_string_unref(kl);

	if (exc == DOM_NO_ERR) {
		args.rval().setBoolean(res);
		return true;
	}
	args.rval().setUndefined();
	return true;
}

static bool
tokenlist_remove(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_tokenlist *tl = JS::GetMaybePtrFromReservedSlot<dom_tokenlist>(hobj, 0);

	if (!tl || argc < 1) {
		args.rval().setUndefined();
		return true;
	}
	char *klass = jsval_to_string(ctx, args[0]);

	if (!klass) {
		args.rval().setUndefined();
		return true;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create(klass, strlen(klass), &kl);
	mem_free(klass);

	if (exc != DOM_NO_ERR || !kl) {
		args.rval().setUndefined();
		return true;
	}
	exc = dom_tokenlist_remove(tl, kl);
	dom_string_unref(kl);

	if (exc == DOM_NO_ERR) {
		interpreter->changed = true;
	}
	args.rval().setUndefined();
	return true;
}

static bool
tokenlist_toggle(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	dom_tokenlist *tl = JS::GetMaybePtrFromReservedSlot<dom_tokenlist>(hobj, 0);

	if (!tl || argc < 1) {
		args.rval().setUndefined();
		return true;
	}
	char *klass = jsval_to_string(ctx, args[0]);

	if (!klass) {
		args.rval().setUndefined();
		return true;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create(klass, strlen(klass), &kl);
	mem_free(klass);

	if (exc != DOM_NO_ERR || !kl) {
		args.rval().setUndefined();
		return true;
	}
	bool res = false;
	exc = dom_tokenlist_contains(tl, kl, &res);

	if (exc == DOM_NO_ERR) {
		if (res) {
			exc = dom_tokenlist_remove(tl, kl);
		} else {
			exc = dom_tokenlist_add(tl, kl);
		}
		if (exc == DOM_NO_ERR) {
			interpreter->changed = true;
		}
	}
	dom_string_unref(kl);
	args.rval().setUndefined();
	return true;
}

JSObject *
getTokenlist(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *el = JS_NewObject(ctx, &tokenlist_class);

	if (!el) {
		return NULL;
	}
	JS::RootedObject r_el(ctx, el);
	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) tokenlist_props);
	spidermonkey_DefineFunctions(ctx, el, tokenlist_funcs);
	JS::SetReservedSlot(el, 0, JS::PrivateValue(node));

	return el;
}
