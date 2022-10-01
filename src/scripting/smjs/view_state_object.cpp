/* Exports struct view_state to the world of ECMAScript */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "ecmascript/spidermonkey-shared.h"
#include "protocol/uri.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/view_state_object.h"
#include "scripting/smjs/core.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "util/error.h"
#include "util/memory.h"
#include "viewer/text/vs.h"

static bool view_state_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool view_state_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static void view_state_finalize(JS::GCContext *op, JSObject *obj);

static const JSClassOps view_state_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	view_state_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	nullptr // trace JS_GlobalObjectTraceHook
};

static const JSClass view_state_class = {
	"view_state",
	JSCLASS_HAS_RESERVED_SLOTS(1),	/* struct view_state * */
	&view_state_ops
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in view_state[-1];
 * future versions of ELinks may change the numbers.  */
enum view_state_prop {
	VIEW_STATE_PLAIN = -1,
	VIEW_STATE_URI   = -2,
};

static bool
view_state_get_property_plain(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &view_state_class, NULL))
		return false;

	vs = JS::GetMaybePtrFromReservedSlot<struct view_state>(hobj, 0);

	if (!vs) return false;

	args.rval().setInt32(vs->plain);

	return true;
}

static bool
view_state_set_property_plain(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;

	if (argc != 1) {
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &view_state_class, NULL))
		return false;

	vs = JS::GetMaybePtrFromReservedSlot<struct view_state>(hobj, 0);

	if (!vs) return false;

	vs->plain = args[0].toInt32();

	return true;
}

static bool
view_state_get_property_uri(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &view_state_class, NULL))
		return false;

	struct view_state *vs = JS::GetMaybePtrFromReservedSlot<struct view_state>(hobj, 0);

	if (!vs) return false;

	args.rval().setString(JS_NewStringCopyZ(ctx, struri(vs->uri)));

	return true;
}

static const JSPropertySpec view_state_props[] = {
	JS_PSGS("plain", view_state_get_property_plain, view_state_set_property_plain, JSPROP_ENUMERATE),
	JS_PSG("uri", view_state_get_property_uri, JSPROP_ENUMERATE),
	JS_PS_END
};

/** Pointed to by view_state_class.finalize.  SpiderMonkey automatically
 * finalizes all objects before it frees the JSRuntime, so view_state.jsobject
 * won't be left dangling.  */
static void
view_state_finalize(JS::GCContext *op, JSObject *obj)
{
	struct view_state *vs;

	vs = JS::GetMaybePtrFromReservedSlot<struct view_state>(obj, 0);

	if (!vs) return; /* already detached */
	assert(vs->jsobject == obj);
	if_assert_failed return;

	JS::SetReservedSlot(obj, 0, JS::UndefinedValue()); /* perhaps not necessary */
	vs->jsobject = NULL;
}


/** Return an SMJS object through which scripts can access @a vs.  If there
 * already is such an object, return that; otherwise create a new one.  */
JSObject *
smjs_get_view_state_object(struct view_state *vs)
{
	JSObject *view_state_object;

	if (vs->jsobject) return vs->jsobject;

	assert(smjs_ctx);
	if_assert_failed return NULL;

	view_state_object = JS_NewObject(smjs_ctx,
	                                  (JSClass *) &view_state_class);

	if (!view_state_object) return NULL;

	JS::RootedObject r_view_state_object(smjs_ctx, view_state_object);

	if (false == JS_DefineProperties(smjs_ctx, r_view_state_object,
	                               (JSPropertySpec *) view_state_props))
		return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @vs.  */

	JS::SetReservedSlot(view_state_object, 0, JS::PrivateValue(vs));	/* to @view_state_class */
	vs->jsobject = view_state_object;

	return view_state_object;
}

static bool
smjs_elinks_get_view_state(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	args.rval().setNull();

	if (!smjs_ses || !have_location(smjs_ses)) return true;

	struct view_state *vs = &cur_loc(smjs_ses)->vs;

	if (!vs) return true;
	JSObject *vs_obj = smjs_get_view_state_object(vs);

	if (!vs_obj) return true;
	args.rval().setObject(*vs_obj);

	return true;
}

/** Ensure that no JSObject contains the pointer @a vs.  This is called from
 * destroy_vs.  If a JSObject was previously attached to the view state, the
 * object will remain in memory but it will no longer be able to access the
 * view state. */
void
smjs_detach_view_state_object(struct view_state *vs)
{
	assert(smjs_ctx);
	assert(vs);
	if_assert_failed return;

	if (!vs->jsobject) return;

	JS::RootedObject robj(smjs_ctx, vs->jsobject);

	if (!JS_InstanceOf(smjs_ctx, robj, (JSClass *) &view_state_class, NULL))
		return;

	JS::SetReservedSlot(vs->jsobject, 0, JS::UndefinedValue());
	vs->jsobject = NULL;
}

void
smjs_init_view_state_interface(void)
{
	if (!smjs_ctx || !smjs_elinks_object)
		return;

	JS::RootedObject r_smjs_elinks_object(smjs_ctx, smjs_elinks_object);
	JS_DefineProperty(smjs_ctx, r_smjs_elinks_object, "vs", smjs_elinks_get_view_state, nullptr,
		(unsigned int)(JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)
	);
}
