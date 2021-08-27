/* "elinks.keymaps" */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/kbdbind.h"
#include "ecmascript/spidermonkey-shared.h"
#include "main/event.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "util/memory.h"

static bool keymap_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool keymap_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static void keymap_finalize(JSFreeOp *op, JSObject *obj);

static const JSClassOps keymap_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	nullptr // trace JS_GlobalObjectTraceHook
};

static const JSClass keymap_class = {
	"keymap",
	JSCLASS_HAS_PRIVATE,	/* int * */
	&keymap_ops
};

/* @keymap_class.getProperty */
static bool
keymap_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	char *action_str;
	const char *keystroke_str;
	int *data;
	JS::Value tmp;
	JS::RootedValue r_tmp(ctx, tmp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &keymap_class, NULL))
		return false;

	data = JS_GetInstancePrivate(ctx, hobj,
				     (JSClass *) &keymap_class, NULL);



	if (!JS_IdToValue(ctx, id, &r_tmp))
		goto ret_null;

	keystroke_str = jsval_to_string(ctx, r_tmp);
	if (!keystroke_str) goto ret_null;

	action_str = get_action_name_from_keystroke((enum keymap_id) *data,
						    keystroke_str);
	if (!action_str || !strcmp(action_str, "none")) goto ret_null;

	hvp.setString(JS_NewStringCopyZ(ctx, action_str));

	return true;

ret_null:
	hvp.setNull();

	return true;
}

static enum evhook_status
smjs_keybinding_action_callback(va_list ap, void *data)
{
	JS::CallArgs args;

	JS::Value rval;
	JS::RootedValue r_rval(smjs_ctx, rval);
	struct session *ses = va_arg(ap, struct session *);
	JSObject *jsobj = data;

	evhook_use_params(ses);

	smjs_ses = ses;

	JS::Value r2;
	JS::RootedValue r_jsobject(smjs_ctx, r2);
	r_jsobject.setObject(*jsobj);

	JS_CallFunctionValue(smjs_ctx, nullptr, r_jsobject,
			     args, &r_rval);

	smjs_ses = NULL;

	return EVENT_HOOK_STATUS_LAST;
}

/* @keymap_class.setProperty */
static bool
keymap_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	int *data;
	char *keymap_str;
	const char *keystroke_str;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &keymap_class, NULL)) {
		return false;
	}

	data = JS_GetInstancePrivate(ctx, hobj,
				     (JSClass *) &keymap_class, NULL);

	/* Ugly fact: we need to get the string from the id to give to bind_do,
	 * which will of course then convert the string back to an id... */
	keymap_str = get_keymap_name((enum keymap_id) *data);

	if (!keymap_str) return false;

	JS::RootedValue rval(ctx);
	JS_IdToValue(ctx, id, &rval);
	keystroke_str = jsval_to_string(ctx, rval);

	if (!keystroke_str) return false;

	if (hvp.isString()) {
		char *action_str;

		action_str = jsval_to_string(ctx, hvp);
		if (!action_str) return false;

		if (bind_do(keymap_str, keystroke_str, action_str, 0))
			return false;

		return true;

	} else if (hvp.isNull()) { /* before JSVAL_IS_OBJECT */

		if (bind_do(keymap_str, keystroke_str, "none", 0))
			return false;

		return true;

	} else if (hvp.isObject() || hvp.isNull()) {
		char *err = NULL;
		int event_id;
		struct string event_name = NULL_STRING;
		JSObject *jsobj = &hvp.toObject();

		if (false == JS_ObjectIsFunction(ctx, jsobj))
			return false;

		if (!init_string(&event_name)) return false;

		add_format_to_string(&event_name, "smjs-run-func %p", jsobj);

		event_id = bind_key_to_event_name(keymap_str,
						  keystroke_str,
						  event_name.source, &err);

		done_string(&event_name);

		if (err) {
			alert_smjs_error(err);

			return false;
		}

		event_id = register_event_hook(event_id,
		                               smjs_keybinding_action_callback,
		                               0, (void *) jsobj);

		if (event_id == EVENT_NONE) {
			alert_smjs_error("error registering event hook"
			                 " for keybinding");

			return false;
		}

		return true;
	}

	return false;
}

/* @keymap_class.finalize */
static void
keymap_finalize(JSFreeOp *op, JSObject *obj)
{
	void *data;
#if 0
	assert(JS_InstanceOf(ctx, obj, (JSClass *) &keymap_class, NULL));
	if_assert_failed return;
#endif

	data = JS_GetPrivate(obj);

	mem_free(data);
}

static JSObject *
smjs_get_keymap_object(enum keymap_id keymap_id)
{
	int *data;
	JSObject *keymap_object;

	assert(smjs_ctx);

	keymap_object = JS_NewObject(smjs_ctx, (JSClass *) &keymap_class);

	if (!keymap_object) return NULL;

	data = intdup(keymap_id);
	if (!data) return NULL;

	JS_SetPrivate(keymap_object, data); /* to @keymap_class */
	return keymap_object;
}

static const JSClassOps keymap_hash_ops = {
	nullptr, nullptr,
	nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr,
};

static const JSClass keymaps_hash_class = {
	"keymaps_hash",
	JSCLASS_HAS_PRIVATE,
	&keymap_hash_ops
};

static JSObject *
smjs_get_keymap_hash_object(void)
{
	int keymap_id;
	JSObject *keymaps_hash;

	keymaps_hash = JS_NewObject(smjs_ctx, (JSClass *) &keymaps_hash_class);
	if (!keymaps_hash) return NULL;

	JS::RootedObject r_keymaps_hash(smjs_ctx, keymaps_hash);
	JS::RootedValue r_val(smjs_ctx);

	for (keymap_id = 0; keymap_id < KEYMAP_MAX; ++keymap_id) {
		char *keymap_str = get_keymap_name(keymap_id);
		JSObject *map = smjs_get_keymap_object(keymap_id);

		assert(keymap_str);

		if (!map) return NULL;

		r_val.setObject(*map);
		JS_SetProperty(smjs_ctx, r_keymaps_hash, keymap_str, r_val);
	}

	return keymaps_hash;
}

void
smjs_init_keybinding_interface(void)
{
	struct JSObject *keymaps_hash;

	if (!smjs_ctx || !smjs_elinks_object)
		return;

	keymaps_hash = smjs_get_keymap_hash_object();
	if (!keymaps_hash) return;

	JS::RootedValue r_val(smjs_ctx);
	r_val.setObject(*keymaps_hash);
	JS::RootedObject r_smjs_elinks_object(smjs_ctx, smjs_elinks_object);

	JS_SetProperty(smjs_ctx, r_smjs_elinks_object, "keymaps", r_val);
}
