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
static bool view_state_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, bool strict, JS::MutableHandleValue hvp);
static void view_state_finalize(JSFreeOp *op, JSObject *obj);

static const JSClass view_state_class = {
	"view_state",
	JSCLASS_HAS_PRIVATE,	/* struct view_state * */
	JS_PropertyStub, JS_DeletePropertyStub,
	view_state_get_property, view_state_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, view_state_finalize
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in view_state[-1];
 * future versions of ELinks may change the numbers.  */
enum view_state_prop {
	VIEW_STATE_PLAIN = -1,
	VIEW_STATE_URI   = -2,
};

static const JSPropertySpec view_state_props[] = {
	{ "plain", (unsigned char)VIEW_STATE_PLAIN, JSPROP_ENUMERATE },
	{ "uri",   (unsigned char)VIEW_STATE_URI,   JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

/* @view_state_class.getProperty */
static bool
view_state_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &view_state_class, NULL))
		return false;

	vs = JS_GetInstancePrivate(ctx, hobj,
				   (JSClass *) &view_state_class, NULL);
	if (!vs) return false;

	hvp.setUndefined();

	if (!JSID_IS_INT(id))
		return false;

	switch (JSID_TO_INT(id)) {
	case VIEW_STATE_PLAIN:
		hvp.setInt32(vs->plain);

		return true;
	case VIEW_STATE_URI:
		hvp.setString(JS_NewStringCopyZ(smjs_ctx, struri(vs->uri)));

		return true;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return true in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		return true;
	}
}

/* @view_state_class.setProperty */
static bool
view_state_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, bool strict, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &view_state_class, NULL))
		return false;

	vs = JS_GetInstancePrivate(ctx, hobj,
				   (JSClass *) &view_state_class, NULL);
	if (!vs) return false;

	if (!JSID_IS_INT(id))
		return false;

	switch (JSID_TO_INT(id)) {
	case VIEW_STATE_PLAIN: {
		vs->plain = hvp.toInt32();

		return true;
	}
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return true in this case.
		 * Do the same here.  */
		return true;
	}
}

/** Pointed to by view_state_class.finalize.  SpiderMonkey automatically
 * finalizes all objects before it frees the JSRuntime, so view_state.jsobject
 * won't be left dangling.  */
static void
view_state_finalize(JSFreeOp *op, JSObject *obj)
{
	struct view_state *vs;
#if 0
	assert(JS_InstanceOf(ctx, obj, (JSClass *) &view_state_class, NULL));
	if_assert_failed return;

	vs = JS_GetInstancePrivate(ctx, obj,
	                           (JSClass *) &view_state_class, NULL);
#endif

	vs = JS_GetPrivate(obj);

	if (!vs) return; /* already detached */

	JS_SetPrivate(obj, NULL); /* perhaps not necessary */
	assert(vs->jsobject == obj);
	if_assert_failed return;
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
	                                  (JSClass *) &view_state_class,
	                                  JS::NullPtr(), JS::NullPtr());

	if (!view_state_object) return NULL;

	JS::RootedObject r_view_state_object(smjs_ctx, view_state_object);

	if (false == JS_DefineProperties(smjs_ctx, r_view_state_object,
	                               (JSPropertySpec *) view_state_props))
		return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @vs.  */
	JS_SetPrivate(view_state_object, vs);	/* to @view_state_class */

	vs->jsobject = view_state_object;
	return view_state_object;
}

static bool
smjs_elinks_get_view_state(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	JSObject *vs_obj;
	struct view_state *vs;

	hvp.setNull();

	if (!smjs_ses || !have_location(smjs_ses)) return true;

	vs = &cur_loc(smjs_ses)->vs;
	if (!vs) return true;

	vs_obj = smjs_get_view_state_object(vs);
	if (!vs_obj) return true;

	hvp.setObject(*vs_obj);

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

	JS::RootedObject r_vs_jsobject(smjs_ctx, vs->jsobject);

	assert(JS_GetInstancePrivate(smjs_ctx, r_vs_jsobject,
				     (JSClass *) &view_state_class, NULL)
	       == vs);
	if_assert_failed {}

	JS_SetPrivate(vs->jsobject, NULL);
	vs->jsobject = NULL;
}

void
smjs_init_view_state_interface(void)
{
	if (!smjs_ctx || !smjs_elinks_object)
		return;

	JS::RootedObject r_smjs_elinks_object(smjs_ctx, smjs_elinks_object);

	JS_DefineProperty(smjs_ctx, r_smjs_elinks_object, "vs", (int32_t)0,
		(unsigned int)(JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY), smjs_elinks_get_view_state, JS_StrictPropertyStub
	);
}
