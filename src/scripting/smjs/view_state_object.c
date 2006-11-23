/* Exports struct view_state to the world of ECMAScript */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
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

enum view_state_prop {
	VIEW_STATE_PLAIN,
	VIEW_STATE_URI,
};

static const JSPropertySpec view_state_props[] = {
	{ "plain", VIEW_STATE_PLAIN, JSPROP_ENUMERATE },
	{ "uri",   VIEW_STATE_URI,   JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};

/* @view_state_class.getProperty */
static JSBool
view_state_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct view_state *vs = JS_GetPrivate(ctx, obj);

	undef_to_jsval(ctx, vp);

	if (!JSVAL_IS_INT(id))
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case VIEW_STATE_PLAIN:
		*vp = INT_TO_JSVAL(vs->plain);

		return JS_TRUE;
	case VIEW_STATE_URI:
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx,
		                                        struri(vs->uri)));

		return JS_TRUE;
	default:
		INTERNAL("Invalid ID %d in view_state_get_property().",
		         JSVAL_TO_INT(id));
	}

	return JS_FALSE;
}

/* @view_state_class.setProperty */
static JSBool
view_state_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct view_state *vs = JS_GetPrivate(ctx, obj);

	if (!JSVAL_IS_INT(id))
		return JS_FALSE;

	switch (JSVAL_TO_INT(id)) {
	case VIEW_STATE_PLAIN: {
		vs->plain = atol(jsval_to_string(ctx, vp));

		return JS_TRUE;
	}
	default:
		INTERNAL("Invalid ID %d in view_state_set_property().",
		         JSVAL_TO_INT(id));
	}

	return JS_FALSE;
}

static const JSClass view_state_class = {
	"view_state",
	JSCLASS_HAS_PRIVATE,	/* struct view_state * */
	JS_PropertyStub, JS_PropertyStub,
	view_state_get_property, view_state_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

JSObject *
smjs_get_view_state_object(struct view_state *vs)
{
	JSObject *view_state_object;

	assert(smjs_ctx);

	view_state_object = JS_NewObject(smjs_ctx,
	                                  (JSClass *) &view_state_class,
	                                  NULL, NULL);

	if (!view_state_object) return NULL;

	if (JS_FALSE == JS_SetPrivate(smjs_ctx, view_state_object, vs))
		return NULL;

	if (JS_FALSE == JS_DefineProperties(smjs_ctx, view_state_object,
	                               (JSPropertySpec *) view_state_props))
		return NULL;

	return view_state_object;
}

static JSBool
smjs_elinks_get_view_state(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
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

void
smjs_init_view_state_interface(void)
{
	if (!smjs_ctx || !smjs_elinks_object)
		return;

	JS_DefineProperty(smjs_ctx, smjs_elinks_object, "vs", JSVAL_NULL,
	                smjs_elinks_get_view_state, JS_PropertyStub,
	                JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
}
