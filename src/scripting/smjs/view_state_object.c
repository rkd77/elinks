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

static const JSClass view_state_class; /* defined below */

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in view_state[-1];
 * future versions of ELinks may change the numbers.  */
enum view_state_prop {
	VIEW_STATE_PLAIN = -1,
	VIEW_STATE_URI   = -2,
};

static const JSPropertySpec view_state_props[] = {
	{ "plain", VIEW_STATE_PLAIN, JSPROP_ENUMERATE },
	{ "uri",   VIEW_STATE_URI,   JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

/* @view_state_class.getProperty */
static JSBool
view_state_get_property(JSContext *ctx, JSObject *obj, jsid id, jsval *vp)
{
	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &view_state_class, NULL))
		return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, obj,
				   (JSClass *) &view_state_class, NULL);
	if (!vs) return JS_FALSE;

	undef_to_jsval(ctx, vp);

	if (!JSID_IS_INT(id))
		return JS_FALSE;

	switch (JSID_TO_INT(id)) {
	case VIEW_STATE_PLAIN:
		*vp = INT_TO_JSVAL(vs->plain);

		return JS_TRUE;
	case VIEW_STATE_URI:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
		                                        struri(vs->uri)));

		return JS_TRUE;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		return JS_TRUE;
	}
}

/* @view_state_class.setProperty */
static JSBool
view_state_set_property(JSContext *ctx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &view_state_class, NULL))
		return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, obj,
				   (JSClass *) &view_state_class, NULL);
	if (!vs) return JS_FALSE;

	if (!JSID_IS_INT(id))
		return JS_FALSE;

	switch (JSID_TO_INT(id)) {
	case VIEW_STATE_PLAIN: {
		vs->plain = atol(jsval_to_string(ctx, vp));

		return JS_TRUE;
	}
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case.
		 * Do the same here.  */
		return JS_TRUE;
	}
}

/** Pointed to by view_state_class.finalize.  SpiderMonkey automatically
 * finalizes all objects before it frees the JSRuntime, so view_state.jsobject
 * won't be left dangling.  */
static void
view_state_finalize(JSContext *ctx, JSObject *obj)
{
	struct view_state *vs;

	assert(JS_InstanceOf(ctx, obj, (JSClass *) &view_state_class, NULL));
	if_assert_failed return;

	vs = JS_GetInstancePrivate(ctx, obj,
	                           (JSClass *) &view_state_class, NULL);

	if (!vs) return; /* already detached */

	JS_SetPrivate(ctx, obj, NULL); /* perhaps not necessary */
	assert(vs->jsobject == obj);
	if_assert_failed return;
	vs->jsobject = NULL;
}

static const JSClass view_state_class = {
	"view_state",
	JSCLASS_HAS_PRIVATE,	/* struct view_state * */
	JS_PropertyStub, JS_PropertyStub,
	view_state_get_property, view_state_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, view_state_finalize
};

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
	                                  NULL, NULL);

	if (!view_state_object) return NULL;

	if (JS_FALSE == JS_DefineProperties(smjs_ctx, view_state_object,
	                               (JSPropertySpec *) view_state_props))
		return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @vs.  */
	if (JS_FALSE == JS_SetPrivate(smjs_ctx, view_state_object, vs))	/* to @view_state_class */
		return NULL;

	vs->jsobject = view_state_object;
	return view_state_object;
}

static JSBool
smjs_elinks_get_view_state(JSContext *ctx, JSObject *obj, jsid id, jsval *vp)
{
	JSObject *vs_obj;
	struct view_state *vs;

	*vp = JSVAL_NULL;

	if (!smjs_ses || !have_location(smjs_ses)) return JS_TRUE;

	vs = &cur_loc(smjs_ses)->vs;
	if (!vs) return JS_TRUE;

	vs_obj = smjs_get_view_state_object(vs);
	if (!vs_obj) return JS_TRUE;

	*vp = OBJECT_TO_JSVAL(vs_obj);

	return JS_TRUE;
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

	assert(JS_GetInstancePrivate(smjs_ctx, vs->jsobject,
				     (JSClass *) &view_state_class, NULL)
	       == vs);
	if_assert_failed {}

	JS_SetPrivate(smjs_ctx, vs->jsobject, NULL);
	vs->jsobject = NULL;
}

void
smjs_init_view_state_interface(void)
{
	if (!smjs_ctx || !smjs_elinks_object)
		return;

	JS_DefineProperty(smjs_ctx, smjs_elinks_object, "vs", JSVAL_NULL,
	                smjs_elinks_get_view_state, JS_StrictPropertyStub,
	                JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
}
