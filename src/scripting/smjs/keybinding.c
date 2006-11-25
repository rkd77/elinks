/* "elinks.keymaps" */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/kbdbind.h"
#include "ecmascript/spidermonkey/util.h"
#include "main/event.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "util/memory.h"

static JSBool
keymap_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	unsigned char *action_str;
	unsigned char *keystroke_str;
	int *data = JS_GetPrivate(ctx, obj);
	enum keymap_id keymap_id = *data;

	keystroke_str = JS_GetStringBytes(JS_ValueToString(ctx, id));
	if (!keystroke_str) goto ret_null;

	action_str = get_action_name_from_keystroke(keymap_id, keystroke_str);
	if (!action_str) goto ret_null;

	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, action_str));

	return JS_TRUE;

ret_null:
	*vp = JSVAL_NULL;

	return JS_TRUE;
}

static enum evhook_status
smjs_keybinding_action_callback(va_list ap, void *data)
{
	jsval rval;
	struct session *ses = va_arg(ap, struct session *);
	JSObject *jsobj = data;
	JSFunction *func = JS_ValueToFunction(smjs_ctx, OBJECT_TO_JSVAL(jsobj));

	evhook_use_params(ses);

	assert(func);

	smjs_ses = ses;

	JS_CallFunction(smjs_ctx, NULL, func, 0, NULL, &rval);

	smjs_ses = NULL;

	return EVENT_HOOK_STATUS_LAST;
}

static JSBool
keymap_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	int *data = JS_GetPrivate(ctx, obj);
	enum keymap_id keymap_id = *data;
	unsigned char *keymap_str;
	unsigned char *keystroke_str;

	/* Ugly fact: we need to get the string from the id to give to bind_do,
	 * which will of course then convert the string back to an id... */
	keymap_str = get_keymap_name(keymap_id);
	if (!keymap_str) return JS_FALSE;

	keystroke_str = JS_GetStringBytes(JS_ValueToString(ctx, id));
	if (!keystroke_str) return JS_FALSE;

	if (JSVAL_IS_STRING(*vp)) {
		unsigned char *action_str;

		action_str = JS_GetStringBytes(JS_ValueToString(ctx, *vp));
		if (!action_str) return JS_FALSE;

		if (bind_do(keymap_str, keystroke_str, action_str, 0))
			return JS_FALSE;

		return JS_TRUE;

	} else if (JSVAL_IS_OBJECT(*vp)) {
		unsigned char *err = NULL;
		int event_id;
		struct string event_name = NULL_STRING;
		JSObject *jsobj = JSVAL_TO_OBJECT(*vp);
		
		if (JS_FALSE == JS_ObjectIsFunction(ctx, jsobj))
			return JS_FALSE;

		if (!init_string(&event_name)) return JS_FALSE;

		add_format_to_string(&event_name, "smjs-run-func %p", jsobj);

		event_id = bind_key_to_event_name(keymap_str,
						  keystroke_str,
						  event_name.source, &err);

		done_string(&event_name);

		if (err) {
			alert_smjs_error(err);

			return JS_FALSE;
		}

		event_id = register_event_hook(event_id,
		                               smjs_keybinding_action_callback,
		                               0, (void *) jsobj);

		if (event_id == EVENT_NONE) {
			alert_smjs_error("error registering event hook"
			                 " for keybinding");

			return JS_FALSE;
		}

		return JS_TRUE;
	}

	return JS_TRUE;
}

static void
keymap_finalize(JSContext *ctx, JSObject *obj)
{
	void *data = JS_GetPrivate(ctx, obj);

	mem_free(data);
}

static const JSClass keymap_class = {
	"keymap",
	JSCLASS_HAS_PRIVATE,	/* int * */
	JS_PropertyStub, JS_PropertyStub,
	keymap_get_property, keymap_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, keymap_finalize,
};

static JSObject *
smjs_get_keymap_object(enum keymap_id keymap_id)
{
	int *data;
	JSObject *keymap_object;
		
	assert(smjs_ctx);

	keymap_object = JS_NewObject(smjs_ctx, (JSClass *) &keymap_class,
	                             NULL, NULL);

	if (!keymap_object) return NULL;

	data = intdup(keymap_id);
	if (!data) return NULL;

	if (JS_TRUE == JS_SetPrivate(smjs_ctx, keymap_object, data))
		return keymap_object;

	mem_free(data);

	return NULL;
}

static const JSClass keymaps_hash_class = {
	"keymaps_hash",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
};

static JSObject *
smjs_get_keymap_hash_object(void)
{
	jsval val;
	enum keymap_id keymap_id;
	JSObject *keymaps_hash;
	
	keymaps_hash = JS_NewObject(smjs_ctx, (JSClass *) &keymaps_hash_class,
	                            NULL, NULL);
	if (!keymaps_hash) return NULL;

	for (keymap_id = 0; keymap_id < KEYMAP_MAX; ++keymap_id) {
		unsigned char *keymap_str = get_keymap_name(keymap_id);
		JSObject *map = smjs_get_keymap_object(keymap_id);

		assert(keymap_str);

		if (!map) return NULL;

		val = OBJECT_TO_JSVAL(map);

		JS_SetProperty(smjs_ctx, keymaps_hash, keymap_str, &val);
	}

	return keymaps_hash;
}

void
smjs_init_keybinding_interface(void)
{
	jsval val;
	struct JSObject *keymaps_hash;

	if (!smjs_ctx || !smjs_elinks_object)
		return;

	keymaps_hash = smjs_get_keymap_hash_object();
	if (!keymaps_hash) return;

	val = OBJECT_TO_JSVAL(keymaps_hash);

	JS_SetProperty(smjs_ctx, smjs_elinks_object, "keymaps", &val);
}
