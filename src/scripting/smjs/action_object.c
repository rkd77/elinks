/* "elinks.action" */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/kbdbind.h"
#include "ecmascript/spidermonkey-shared.h"
#include "intl/libintl.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "session/session.h"
#include "terminal/window.h"
#include "util/memory.h"
#include "viewer/action.h"

/*** action method object ***/

struct smjs_action_fn_callback_hop {
	struct session *ses;
	action_id_T action_id;
};

static void smjs_action_fn_finalize(JSFreeOp *op, JSObject *obj);
static bool smjs_action_fn_callback(JSContext *ctx, unsigned int argc, JS::Value *rval);

static JSClassOps action_fn_ops = {
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr,
	smjs_action_fn_finalize,
	NULL,
	smjs_action_fn_callback,
};

static const JSClass action_fn_class = {
	"action_fn",
	JSCLASS_HAS_PRIVATE,	/* struct smjs_action_fn_callback_hop * */
	&action_fn_ops
};

/* @action_fn_class.finalize */
static void
smjs_action_fn_finalize(JSFreeOp *op, JSObject *obj)
{
	struct smjs_action_fn_callback_hop *hop;

#if 0
	assert(JS_InstanceOf(ctx, obj, (JSClass *) &action_fn_class, NULL));
	if_assert_failed return;

	hop = JS_GetInstancePrivate(ctx, obj,
				    (JSClass *) &action_fn_class, NULL);
#endif

	hop = JS_GetPrivate(obj);

	mem_free_if(hop);
}

/* @action_fn_class.call */
static bool
smjs_action_fn_callback(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::Value value;
	JS::CallArgs args = CallArgsFromVp(argc, rval);

	struct smjs_action_fn_callback_hop *hop;
	JS::RootedObject fn_obj(ctx);

	assert(smjs_ctx);
	if_assert_failed return false;

//	if (true != JS_ValueToObject(ctx, JS_CALLEE(ctx, rval), &fn_obj)) {
	if (true != JS_ValueToObject(ctx, args[0], &fn_obj)) {
		args.rval().setBoolean(false);
		return true;
	}
	assert(JS_InstanceOf(ctx, fn_obj, (JSClass *) &action_fn_class, NULL));
	if_assert_failed return false;

	hop = JS_GetInstancePrivate(ctx, fn_obj,
				    (JSClass *) &action_fn_class, NULL);
	if (!hop) {
		args.rval().setBoolean(false);
		return true;
	}

	if (!would_window_receive_keypresses(hop->ses->tab)) {
		/* The user cannot run actions in this tab by pressing
		 * keys, and some actions could crash if run in this
		 * situation; so we don't let user scripts run actions
		 * either.
		 *
		 * In particular, this check should fix bug 943, where
		 * ::ACT_MAIN_TAB_CLOSE called destroy_session(),
		 * which freed struct type_query while BFU dialogs had
		 * pointers to it.  That crash could be prevented in
		 * various ways but it seems other similar crashes are
		 * possible, e.g. if the link menu is open and has a
		 * pointer to a session that is then destroyed.
		 * Instead of thoroughly auditing the use of pointers
		 * to sessions and related structures, I'll just
		 * disable the feature, to bring the ELinks 0.12
		 * release closer.
		 *
		 * The "%s" prevents interpretation of any percent
		 * signs in translations.  */
		JS_ReportErrorUTF8(ctx, "%s",
			       _("Cannot run actions in a tab that doesn't "
				 "have the focus", hop->ses->tab->term));
		return false; /* make JS propagate the exception */
	}

	if (argc >= 1) {
		int32_t val = args[0].toInt32();
		set_kbd_repeat_count(hop->ses, val);
	}

	do_action(hop->ses, hop->action_id, 1);
	args.rval().setBoolean(true);

	return true;
}


static JSObject *
smjs_get_action_fn_object(char *action_str)
{
	char *c;
	struct smjs_action_fn_callback_hop *hop;
	JSObject *obj;

	if (!smjs_ses) return NULL;

	obj = JS_NewObject(smjs_ctx, (JSClass *) &action_fn_class);
	if (!obj) return NULL;

	hop = mem_alloc(sizeof(*hop));
	if (!hop) return NULL;

	hop->ses = smjs_ses;

	/* ECMAScript methods cannot have hyphens in the name, so one should
	 * use underscores instead; here, we must convert them back to hyphens
	 * for the action code. */
	for (c = action_str; *c; ++c) if (*c == '_') *c = '-';

	hop->action_id = get_action_from_string(KEYMAP_MAIN, action_str);

	if (-1 != hop->action_id) {
		JS_SetPrivate(obj, hop); /* to @action_fn_class */
		return obj;
	}

	mem_free(hop);
	return NULL;
}


/*** elinks.action object ***/

/* @action_class.getProperty */
static bool
action_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	JS::Value val;
	JS::RootedValue rval(ctx, val);
	JSObject *action_fn;
	char *action_str;

	hvp.setNull();

	JS_IdToValue(ctx, id, &rval);
	action_str = JS_EncodeString(ctx, rval.toString());
	if (!action_str) return true;

	action_fn = smjs_get_action_fn_object(action_str);
	if (!action_fn) return true;

	hvp.setObject(*action_fn);

	return true;
}

static JSClassOps action_ops = {
	JS_PropertyStub, nullptr,
	action_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr,
};

static const JSClass action_class = {
	"action",
	0,
	&action_ops
};

static JSObject *
smjs_get_action_object(void)
{
	JSObject *obj;

	assert(smjs_ctx);

	obj = JS_NewObject(smjs_ctx, (JSClass *) &action_class);

	return obj;
}

void
smjs_init_action_interface(void)
{
	JS::Value val;
	struct JSObject *action_object;

	if (!smjs_ctx || !smjs_elinks_object)
		return;

	action_object = smjs_get_action_object();
	if (!action_object) return;

	JS::RootedObject r_smjs_elinks_object(smjs_ctx, smjs_elinks_object);

	JS::RootedValue r_val(smjs_ctx, val);
	r_val.setObject(*r_smjs_elinks_object);

	JS_SetProperty(smjs_ctx, r_smjs_elinks_object, "action", r_val);
}
