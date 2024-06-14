/* The SpiderMonkey URLSearchParams object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include <js/BigInt.h>
#include <js/Conversions.h>
#include <js/MapAndSet.h>

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
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/urlsearchparams.h"
#include "ecmascript/timer.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/qs_parse/qs_parse.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#include <iostream>
#include <list>
#include <map>
#include <utility>
#include <sstream>
#include <vector>

struct eljs_urlSearchParams {
	JS::Heap<JSObject *> map;
};

static bool url_get_property_hash(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_set_property_hash(JSContext *ctx, unsigned int argc, JS::Value *vp);

static void
urlSearchParams_finalize(JS::GCContext *op, JSObject *u_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(u_obj, 0);

	if (u) {
		mem_free(u);
	}

}

JSClassOps urlSearchParams_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	urlSearchParams_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass urlSearchParams_class = {
	"URLSearchParams",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&urlSearchParams_ops
};

static void
parse_text(JSContext *ctx, JS::HandleObject obj, char *str)
{
	if (!str || !*str) {
		return;
	}

	char *kvpairs[1024];
	int i = qs_parse(str, kvpairs, 1024);
	int j;

	for (j = 0; j < i; j++) {
		char *key = kvpairs[j];
		char *value = strchr(key, '=');
		if (value) {
			*value++ = '\0';
		}
		JS::RootedValue k(ctx, JS::StringValue(JS_NewStringCopyZ(ctx, key)));
		JS::RootedValue v(ctx, JS::StringValue(JS_NewStringCopyZ(ctx, value)));

		JS::MapSet(ctx, obj, k, v);
	}
}

bool
urlSearchParams_constructor(JSContext* ctx, unsigned argc, JS::Value* vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject newObj(ctx, JS_NewObjectForConstructor(ctx, &urlSearchParams_class, args));
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	if (!newObj) {
		return false;
	}
	struct eljs_urlSearchParams *u = (struct eljs_urlSearchParams *)mem_calloc(1, sizeof(*u));

	if (!u) {
		return false;
	}
	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(u));

	if (argc > 0) {
		char *urlstring = jsval_to_string(ctx, args[0]);

		if (!urlstring) {
			return false;
		}
		JS::RootedObject r(ctx, JS::NewMapObject(ctx));
		u->map = r;
		parse_text(ctx, r, urlstring);
		mem_free(urlstring);
	}
	args.rval().setObject(*newObj);

	return true;
}

static bool
urlSearchParams_get_property_size(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::RootedObject m(ctx, u->map);
	uint32_t size = JS::MapSize(ctx, m);
	args.rval().setInt32(size);

	return true;
}

JSPropertySpec urlSearchParams_props[] = {
	JS_PSG("size",	urlSearchParams_get_property_size, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
urlSearchParams_delete(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);

	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc == 1) {
		JS::RootedObject r(ctx, u->map);
		bool res;
		JS::MapDelete(ctx, r, args[0], &res);
	}
	args.rval().setUndefined();
	return true;
}

static bool
urlSearchParams_entries(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);

	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::RootedObject r(ctx, u->map);
	JS::RootedValue res(ctx);
	JS::MapEntries(ctx, r, &res);
	args.rval().set(res);
	return true;
}

static bool
urlSearchParams_forEach(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);

	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::RootedObject r(ctx, u->map);

	if (argc < 1) {
		args.rval().setUndefined();
		return true;
	}
	JS::MapForEach(ctx, r, args[0], args[1]);
	args.rval().setUndefined();
	return true;
}

static bool
urlSearchParams_get(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);

	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc == 1) {
		JS::RootedObject r(ctx, u->map);
		JS::RootedValue res(ctx);
		JS::MapGet(ctx, r, args[0], &res);
		args.rval().set(res);
		return true;
	}
	args.rval().setUndefined();
	return true;
}

static bool
urlSearchParams_has(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);

	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc == 1) {
		JS::RootedObject r(ctx, u->map);
		bool res = false;
		JS::MapHas(ctx, r, args[0], &res);
		args.rval().setBoolean(res);
		return true;
	}

	if (argc > 1) {
		JS::RootedObject r(ctx, u->map);
		JS::RootedValue res(ctx);
		JS::MapGet(ctx, r, args[0], &res);
		args.rval().setBoolean(res == args[1]);
		return true;
	}
	args.rval().setBoolean(false);
	return true;
}

static bool
urlSearchParams_keys(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);

	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::RootedObject r(ctx, u->map);
	JS::RootedValue res(ctx);
	JS::MapKeys(ctx, r, &res);
	args.rval().set(res);
	return true;
}

static bool
urlSearchParams_set(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);

	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	if (argc == 2) {
		JS::RootedObject r(ctx, u->map);
		JS::MapSet(ctx, r, args[0], args[1]);
	}
	args.rval().setUndefined();
	return true;
}

static struct string result;
static char *prepend;

static bool
map_foreach_callback(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	char *val = jsval_to_string(ctx, args[0]);
	char *key = jsval_to_string(ctx, args[1]);

	add_to_string(&result, prepend);

	if (key) {
		add_to_string(&result, key);
		add_char_to_string(&result, '=');
		mem_free(key);
	}

	if (val) {
		add_to_string(&result, val);
		mem_free(val);
	}
	args.rval().setUndefined();
	prepend = "&";
	return true;
}

static bool
urlSearchParams_toString(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);

	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	if (!init_string(&result)) {
		return false;
	}
	prepend = "";
	JS::RootedObject r(ctx, u->map);
	JS::RootedValue f(ctx);
	JSFunction* fun = JS_NewFunction(ctx, map_foreach_callback, 0, 0, "f");

	if (!fun) {
		return false;
	}
	JS::RootedValue ff(ctx, JS::ObjectValue(*JS_GetFunctionObject(fun)));
	JS::MapForEach(ctx, r, ff, r_val);

	args.rval().setString(JS_NewStringCopyZ(ctx, result.source));
	done_string(&result);
	return true;
}

static bool
urlSearchParams_values(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);

	if (!JS_InstanceOf(ctx, hobj, &urlSearchParams_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_urlSearchParams *u = JS::GetMaybePtrFromReservedSlot<struct eljs_urlSearchParams>(hobj, 0);

	if (!u) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::RootedObject r(ctx, u->map);
	JS::RootedValue res(ctx);
	JS::MapValues(ctx, r, &res);
	args.rval().set(res);
	return true;
}


const spidermonkeyFunctionSpec urlSearchParams_funcs[] = {
	{ "delete",			urlSearchParams_delete,		1 },
	{ "entries",		urlSearchParams_entries,	0 },
	{ "forEach",		urlSearchParams_forEach,	2 },
	{ "get",			urlSearchParams_get,		1 },
	{ "has",			urlSearchParams_has, 		2 },
	{ "keys",			urlSearchParams_keys,		0 },
	{ "set",			urlSearchParams_set, 		2 },
	{ "toString",		urlSearchParams_toString,	0 },
	{ "values",			urlSearchParams_values,		0 },
	{ NULL }
};
