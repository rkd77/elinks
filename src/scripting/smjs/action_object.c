/* "elinks.action" */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/kbdbind.h"
#include "ecmascript/spidermonkey-shared.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "session/session.h"
#include "util/memory.h"
#include "viewer/action.h"

/*** action method object ***/

struct smjs_action_fn_callback_hop {
	struct session *ses;
	action_id_T action_id;
};

static const JSClass action_fn_class; /* defined below */

/* @action_fn_class.finalize */
static void
smjs_action_fn_finalize(JSContext *ctx, JSObject *obj)
{
	struct smjs_action_fn_callback_hop *hop;

	assert(JS_InstanceOf(ctx, obj, (JSClass *) &action_fn_class, NULL));
	if_assert_failed return;

	hop = JS_GetInstancePrivate(ctx, obj,
				    (JSClass *) &action_fn_class, NULL);

	if (hop) mem_free(hop);
}

/* @action_fn_class.call */
static JSBool
smjs_action_fn_callback(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
                        jsval *rval)
{
	struct smjs_action_fn_callback_hop *hop;
	JSObject *fn_obj;

	assert(smjs_ctx);
	if_assert_failed return JS_FALSE;

	*rval = JS_FALSE;

	if (JS_TRUE != JS_ValueToObject(ctx, argv[-2], &fn_obj))
		return JS_TRUE;
	assert(JS_InstanceOf(ctx, fn_obj, (JSClass *) &action_fn_class, NULL));
	if_assert_failed return JS_FALSE;

	hop = JS_GetInstancePrivate(ctx, fn_obj,
				    (JSClass *) &action_fn_class, NULL);
	if (!hop) return JS_TRUE;

	if (argc >= 1) {
		int32 val;

		if (JS_TRUE == JS_ValueToInt32(smjs_ctx, argv[0], &val)) {
			hop->ses->kbdprefix.repeat_count = val;
		}
	}

	do_action(hop->ses, hop->action_id, 1);

	*rval = JS_TRUE;

	return JS_TRUE;
}

static const JSClass action_fn_class = {
	"action_fn",
	JSCLASS_HAS_PRIVATE,	/* struct smjs_action_fn_callback_hop * */
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
	smjs_action_fn_finalize,
	NULL, NULL,
	smjs_action_fn_callback,
};

static JSObject *
smjs_get_action_fn_object(unsigned char *action_str)
{
	unsigned char *c;
	struct smjs_action_fn_callback_hop *hop;
	JSObject *obj;

	if (!smjs_ses) return NULL;

	obj = JS_NewObject(smjs_ctx, (JSClass *) &action_fn_class, NULL, NULL);
	if (!obj) return NULL;

	hop = mem_alloc(sizeof(*hop));
	if (!hop) return NULL;

	hop->ses = smjs_ses;

	/* ECMAScript methods cannot have hyphens in the name, so one should
	 * use underscores instead; here, we must convert them back to hyphens
	 * for the action code. */
	for (c = action_str; *c; ++c) if (*c == '_') *c = '-';

	hop->action_id = get_action_from_string(KEYMAP_MAIN, action_str);

	if (-1 != hop->action_id
	    && JS_TRUE == JS_SetPrivate(smjs_ctx, obj, hop)) { /* to @action_fn_class */
	    	return obj;
	}

	mem_free(hop);
	return NULL;
}


/*** elinks.action object ***/

/* @action_class.getProperty */
static JSBool
action_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *action_fn;
	unsigned char *action_str;

	*vp = JSVAL_NULL;

	action_str = JS_GetStringBytes(JS_ValueToString(ctx, id));
	if (!action_str) return JS_TRUE;

	action_fn = smjs_get_action_fn_object(action_str);
	if (!action_fn) return JS_TRUE;

	*vp = OBJECT_TO_JSVAL(action_fn);

	return JS_TRUE;
}

static const JSClass action_class = {
	"action",
	0,
	JS_PropertyStub, JS_PropertyStub,
	action_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSObject *
smjs_get_action_object(void)
{
	JSObject *obj;

	assert(smjs_ctx);

	obj = JS_NewObject(smjs_ctx, (JSClass *) &action_class, NULL, NULL);

	return obj;
}

void
smjs_init_action_interface(void)
{
	jsval val;
	struct JSObject *action_object;

	if (!smjs_ctx || !smjs_elinks_object)
		return;

	action_object = smjs_get_action_object();
	if (!action_object) return;

	val = OBJECT_TO_JSVAL(action_object);

	JS_SetProperty(smjs_ctx, smjs_elinks_object, "action", &val);
}
