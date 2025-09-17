/* The SpiderMonkey perfomance object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/spidermonkey/util.h"
#include <jsfriendapi.h>

#include "js/ecmascript.h"
#include "js/spidermonkey/performance.h"
#include "osdep/osdep.h"

static void
performance_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
}

JSClassOps performance_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	performance_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

static JSClass performance_class = {
	"performance",
	JSCLASS_HAS_RESERVED_SLOTS(0),
	&performance_ops
};

static bool performance_get_property_timeOrigin(JSContext *ctx, unsigned int argc, JS::Value *vp);

static JSPropertySpec performance_props[] = {
	JS_PSG("timeOrigin",	performance_get_property_timeOrigin, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
performance_get_property_timeOrigin(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &performance_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setNumber(interpreter->time_origin);

	return true;
}

static bool performance_now(JSContext *ctx, unsigned int argc, JS::Value *rval);

static const spidermonkeyFunctionSpec performance_funcs[] = {
	{ "now",	performance_now,	0 },
	{ NULL }
};

static bool
performance_now(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, rval);
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
	if (!JS_InstanceOf(ctx, hobj, &performance_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setNumber(get_time() - interpreter->time_origin);

	return true;
}

JSObject *
getPerformance(JSContext *ctx)
{
	JSObject *el = JS_NewObject(ctx, &performance_class);

	if (!el) {
		return NULL;
	}
	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) performance_props);
	spidermonkey_DefineFunctions(ctx, el, performance_funcs);

	return el;
}
