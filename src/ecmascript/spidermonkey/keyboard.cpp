/* The SpiderMonkey KeyboardEvent object implementation. */

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

#include "ecmascript/libdom/dom.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/libdom/doc.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/keyboard.h"
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


static bool keyboardEvent_get_property_code(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_key(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_keyCode(JSContext *cx, unsigned int argc, JS::Value *vp);

static bool keyboardEvent_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp);
//static bool keyboardEvent_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp);

static bool keyboardEvent_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp);


static term_event_key_T keyCode;

static void
keyboardEvent_finalize(JS::GCContext *op, JSObject *keyb_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_keyboard_event *event = JS::GetMaybePtrFromReservedSlot<dom_keyboard_event>(keyb_obj, 0);

	if (event) {
		dom_event_unref(&event);
	}
}

JSClassOps keyboardEvent_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	keyboardEvent_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass keyboardEvent_class = {
	"KeyboardEvent",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&keyboardEvent_ops
};

bool
keyboardEvent_constructor(JSContext* ctx, unsigned argc, JS::Value* vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject newObj(ctx, JS_NewObjectForConstructor(ctx, &keyboardEvent_class, args));
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
	dom_keyboard_event *event = NULL;
	dom_exception exc = dom_keyboard_event_create(&event);

	if (exc != DOM_NO_ERR) {
		return false;
	}
	dom_string *typ = NULL;

	if (argc > 0) {
		char *t = jsval_to_string(ctx, args[0]);

		if (t) {
			exc = dom_string_create(t, strlen(t), &typ);
			mem_free(t);
		}
	}
	bool bubbles = false;
	bool cancelable = false;
	dom_string *key = NULL;
	dom_string *code = NULL;

	if (argc > 1) {
		JS::RootedValue v(ctx);
		JS::RootedObject v_obj(ctx, &args[1].toObject());

		if (JS_GetProperty(ctx, v_obj, "bubbles", &v)) {
			bubbles = v.toBoolean();
		}
		if (JS_GetProperty(ctx, v_obj, "cancelable", &v)) {
			cancelable = v.toBoolean();
		}
//		if (JS_GetProperty(ctx, v_obj, "composed", &v)) {
//			keyb->composed = (unsigned int)v.toBoolean();
//		}
		if (JS_GetProperty(ctx, v_obj, "key", &v)) {
			char *k = jsval_to_string(ctx, v);

			if (k) {
				exc = dom_string_create(k, strlen(k), &key);
				mem_free(k);
			}
		}
		if (JS_GetProperty(ctx, v_obj, "code", &v)) {
			char *c = jsval_to_string(ctx, v);

			if (c) {
				exc = dom_string_create(c, strlen(c), &code);
				mem_free(c);
			}
		}
	}
	exc = dom_keyboard_event_init(event, typ, bubbles, cancelable, NULL/*view*/,
		key, code, DOM_KEY_LOCATION_STANDARD,
		false, false, false,
		false, false, false);
	if (typ) dom_string_unref(typ);
	if (key) dom_string_unref(key);
	if (code) dom_string_unref(code);

	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(event));
	args.rval().setObject(*newObj);

	return true;
}

JSPropertySpec keyboardEvent_props[] = {
	JS_PSG("bubbles",	keyboardEvent_get_property_bubbles, JSPROP_ENUMERATE),
	JS_PSG("cancelable",	keyboardEvent_get_property_cancelable, JSPROP_ENUMERATE),
	JS_PSG("code",	keyboardEvent_get_property_code, JSPROP_ENUMERATE),
//	JS_PSG("composed",	keyboardEvent_get_property_composed, JSPROP_ENUMERATE),
	JS_PSG("defaultPrevented",	keyboardEvent_get_property_defaultPrevented, JSPROP_ENUMERATE),
	JS_PSG("key",	keyboardEvent_get_property_key, JSPROP_ENUMERATE),
	JS_PSG("keyCode",	keyboardEvent_get_property_keyCode, JSPROP_ENUMERATE),
	JS_PSG("type",	keyboardEvent_get_property_type, JSPROP_ENUMERATE),
	JS_PS_END
};

const spidermonkeyFunctionSpec keyboardEvent_funcs[] = {
	{ "preventDefault",	keyboardEvent_preventDefault,	0 },
	{ NULL }
};

static bool
keyboardEvent_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_keyboard_event *event = JS::GetMaybePtrFromReservedSlot<dom_keyboard_event>(hobj, 0);

	if (!event) {
		return false;
	}
	bool bubbles = false;
	dom_exception exc = dom_event_get_bubbles(event, &bubbles);
	args.rval().setBoolean(bubbles);

	return true;
}

static bool
keyboardEvent_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_keyboard_event *event = JS::GetMaybePtrFromReservedSlot<dom_keyboard_event>(hobj, 0);

	if (!event) {
		return false;
	}
	bool cancelable = false;
	dom_exception exc = dom_event_get_cancelable(event, &cancelable);
	args.rval().setBoolean(cancelable);

	return true;
}

#if 0
static bool
keyboardEvent_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(hobj, 0);

	if (!keyb) {
		return false;
	}
	args.rval().setBoolean(keyb->composed);

	return true;
}
#endif

static bool
keyboardEvent_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_keyboard_event *event = JS::GetMaybePtrFromReservedSlot<dom_keyboard_event>(hobj, 0);

	if (!event) {
		return false;
	}
	bool prevented = false;
	dom_exception exc = dom_event_is_default_prevented(event, &prevented);
	args.rval().setBoolean(prevented);

	return true;
}

static bool
keyboardEvent_get_property_key(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_keyboard_event *event = JS::GetMaybePtrFromReservedSlot<dom_keyboard_event>(hobj, 0);

	if (!event) {
		return false;
	}
	dom_string *key = NULL;
	dom_exception exc = dom_keyboard_event_get_key(event, &key);

	if (exc != DOM_NO_ERR || !key) {
		return false;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(key)));
	dom_string_unref(key);

	return true;
}

static bool
keyboardEvent_get_property_keyCode(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_keyboard_event *event = JS::GetMaybePtrFromReservedSlot<dom_keyboard_event>(hobj, 0);

	if (!event) {
		return false;
	}
	dom_string *key = NULL;
	dom_exception exc = dom_keyboard_event_get_key(event, &key);

	if (exc != DOM_NO_ERR) {
		return false;
	}
	unicode_val_T keyCode = convert_dom_string_to_keycode(key);
	args.rval().setInt32(keyCode);
	if (key) dom_string_unref(key);

	return true;
}

static bool
keyboardEvent_get_property_code(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_keyboard_event *event = JS::GetMaybePtrFromReservedSlot<dom_keyboard_event>(hobj, 0);

	if (!event) {
		return false;
	}
	dom_string *code = NULL;
	dom_exception exc = dom_keyboard_event_get_code(event, &code);

	if (exc != DOM_NO_ERR || !code) {
		return false;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(code)));
	dom_string_unref(code);

	return true;
}

static bool
keyboardEvent_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_keyboard_event *keyb = JS::GetMaybePtrFromReservedSlot<dom_keyboard_event>(hobj, 0);

	if (!keyb) {
		return false;
	}
	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(keyb, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(typ)));
	dom_string_unref(typ);

	return true;
}

static bool
keyboardEvent_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_keyboard_event *event = JS::GetMaybePtrFromReservedSlot<dom_keyboard_event>(hobj, 0);

	if (!event) {
		return false;
	}
	dom_event_prevent_default(event);
	args.rval().setUndefined();

	return true;
}

JSObject *
get_keyboardEvent(JSContext *ctx, struct term_event *ev)
{
	JSObject *k = JS_NewObject(ctx, &keyboardEvent_class);

	if (!k) {
		return NULL;
	}

	JS::RootedObject r_keyb(ctx, k);
	JS_DefineProperties(ctx, r_keyb, (JSPropertySpec *) keyboardEvent_props);

	dom_keyboard_event *keyb = NULL;
	dom_exception exc = dom_keyboard_event_create(&keyb);

	if (exc != DOM_NO_ERR) {
		return NULL;
	}
	term_event_key_T keyCode = get_kbd_key(ev);

	dom_string *dom_key = NULL;
	convert_key_to_dom_string(keyCode, &dom_key);

	dom_string *keydown = NULL;
	exc = dom_string_create("keydown", strlen("keydown"), &keydown);

	exc = dom_keyboard_event_init(keyb, keydown, false, false,
		NULL, dom_key, NULL, DOM_KEY_LOCATION_STANDARD,
		false, false, false, false,
		false, false);

	if (dom_key) {
		dom_string_unref(dom_key);
	}
	if (keydown) {
		dom_string_unref(keydown);
	}
//	keyb->keyCode = keyCode;
	JS::SetReservedSlot(k, 0, JS::PrivateValue(keyb));

	return k;
}
