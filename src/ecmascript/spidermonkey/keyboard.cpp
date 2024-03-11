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

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
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


static bool keyboardEvent_get_property_key(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_keyCode(JSContext *cx, unsigned int argc, JS::Value *vp);

static bool keyboardEvent_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool keyboardEvent_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp);

static bool keyboardEvent_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp);


static unicode_val_T keyCode;

struct keyboard {
	unicode_val_T keyCode;
	char *type_;
	unsigned int bubbles:1;
	unsigned int cancelable:1;
	unsigned int composed:1;
	unsigned int defaultPrevented:1;
};

static void
keyboardEvent_finalize(JS::GCContext *op, JSObject *keyb_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(keyb_obj, 0);

	if (keyb) {
		mem_free_if(keyb->type_);
		mem_free(keyb);
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
	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		return false;
	}

	if (argc > 0) {
		keyb->type_ = jsval_to_string(ctx, args[0]);
	}

	if (argc > 1) {
		JS::RootedValue v(ctx);
		JS::RootedObject v_obj(ctx, &args[1].toObject());

		if (JS_GetProperty(ctx, v_obj, "bubbles", &v)) {
			keyb->bubbles = (unsigned int)v.toBoolean();
		}
		if (JS_GetProperty(ctx, v_obj, "cancelable", &v)) {
			keyb->cancelable = (unsigned int)v.toBoolean();
		}
		if (JS_GetProperty(ctx, v_obj, "composed", &v)) {
			keyb->composed = (unsigned int)v.toBoolean();
		}
		if (JS_GetProperty(ctx, v_obj, "keyCode", &v)) {
			keyb->keyCode = v.toInt32();
		} else {
			keyb->keyCode = 0;
		}
	}
	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(keyb));
	args.rval().setObject(*newObj);

	return true;
}

JSPropertySpec keyboardEvent_props[] = {
	JS_PSG("bubbles",	keyboardEvent_get_property_bubbles, JSPROP_ENUMERATE),
	JS_PSG("cancelable",	keyboardEvent_get_property_cancelable, JSPROP_ENUMERATE),
	JS_PSG("composed",	keyboardEvent_get_property_composed, JSPROP_ENUMERATE),
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
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(hobj, 0);

	if (!keyb) {
		return false;
	}
	args.rval().setBoolean(keyb->bubbles);

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
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(hobj, 0);

	if (!keyb) {
		return false;
	}
	args.rval().setBoolean(keyb->cancelable);

	return true;
}

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
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(hobj, 0);

	if (!keyb) {
		return false;
	}
	args.rval().setBoolean(keyb->defaultPrevented);

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
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(hobj, 0);

	if (!keyb) {
		return false;
	}
	char text[8] = {0};

	*text = keyb->keyCode;
	args.rval().setString(JS_NewStringCopyZ(ctx, text));

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
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(hobj, 0);

	if (!keyb) {
		return false;
	}
	args.rval().setInt32(keyb->keyCode);

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
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(hobj, 0);

	if (!keyb) {
		return false;
	}

	if (!keyb->type_) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, keyb->type_));

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
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(hobj, 0);

	if (!keyb) {
		return false;
	}
	if (keyb->cancelable) {
		keyb->defaultPrevented = 1;
	}
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

	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		return NULL;
	}
	keyCode = get_kbd_key(ev);

	if (keyCode == KBD_ENTER) {
		keyCode = 13;
	}
	keyb->keyCode = keyCode;
	JS::SetReservedSlot(k, 0, JS::PrivateValue(keyb));

	return k;
}
