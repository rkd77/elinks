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

static unicode_val_T keyCode;

struct keyboard {
	unicode_val_T keyCode;
};

static void
keyboardEvent_finalize(JS::GCContext *op, JSObject *keyb_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(keyb_obj, 0);

	if (keyb) {
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!newObj) {
		return false;
	}
	struct keyboard *keyb = (struct keyboard *)mem_calloc(1, sizeof(*keyb));

	if (!keyb) {
		return false;
	}
	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(keyb));
	keyb->keyCode = keyCode;
	args.rval().setObject(*newObj);

	return true;
}

JSPropertySpec keyboardEvent_props[] = {
	JS_PSG("key",	keyboardEvent_get_property_key, JSPROP_ENUMERATE),
	JS_PSG("keyCode",	keyboardEvent_get_property_keyCode, JSPROP_ENUMERATE),
	JS_PS_END
};

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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct keyboard *keyb = JS::GetMaybePtrFromReservedSlot<struct keyboard>(hobj, 0);

	if (!keyb) {
		return false;
	}
	args.rval().setInt32(keyb->keyCode);

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
	keyCode = keyb->keyCode = get_kbd_key(ev);
	JS::SetReservedSlot(k, 0, JS::PrivateValue(keyb));

	return k;
}
