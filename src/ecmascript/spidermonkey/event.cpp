/* The SpiderMonkey Event object implementation. */

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
#include "ecmascript/libdom/dom.h"
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/element.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/event.h"
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

static bool event_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool event_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp);
//static bool event_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool event_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool event_get_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool event_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp);

static bool event_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp);

static void
event_finalize(JS::GCContext *op, JSObject *event_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	dom_event *event = JS::GetMaybePtrFromReservedSlot<dom_event>(event_obj, 0);

	if (event) {
		dom_event_unref(event);
	}
}

JSClassOps event_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	event_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass event_class = {
	"Event",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&event_ops
};

bool
event_constructor(JSContext* ctx, unsigned argc, JS::Value* vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject newObj(ctx, JS_NewObjectForConstructor(ctx, &event_class, args));
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
	dom_event *event = NULL;
	dom_exception exc = dom_event_create(&event);

	if (exc != DOM_NO_ERR) {
		return false;
	}
	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(event));

	dom_string *typ = NULL;

	if (argc > 0) {
		char *tt = jsval_to_string(ctx, args[0]);

		if (tt) {
			exc = dom_string_create((const uint8_t *)tt, strlen(tt), &typ);
			mem_free(tt);
		}
	}
	bool bubbles = false;
	bool cancelable = false;

	if (argc > 1) {
		JS::RootedValue v(ctx);
		JS::RootedObject v_obj(ctx, &args[1].toObject());

		if (JS_GetProperty(ctx, v_obj, "bubbles", &v)) {
			bubbles = (unsigned int)v.toBoolean();
		}
		if (JS_GetProperty(ctx, v_obj, "cancelable", &v)) {
			cancelable = (unsigned int)v.toBoolean();
		}
//		if (JS_GetProperty(ctx, v_obj, "composed", &v)) {
//			event->composed = (unsigned int)v.toBoolean();
//		}
	}
	exc = dom_event_init(event, typ, bubbles, cancelable);
	if (typ) {
		dom_string_unref(typ);
	}
	args.rval().setObject(*newObj);

	return true;
}

JSPropertySpec event_props[] = {
	JS_PSG("bubbles",	event_get_property_bubbles, JSPROP_ENUMERATE),
	JS_PSG("cancelable",	event_get_property_cancelable, JSPROP_ENUMERATE),
	JS_PSG("defaultPrevented",	event_get_property_defaultPrevented, JSPROP_ENUMERATE),
	JS_PSG("target", event_get_property_target, JSPROP_ENUMERATE),
	JS_PSG("type",	event_get_property_type, JSPROP_ENUMERATE),
	JS_PS_END
};

const spidermonkeyFunctionSpec event_funcs[] = {
	{ "preventDefault",	event_preventDefault,	0 },
	{ NULL }
};

static bool
event_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_event *event = JS::GetMaybePtrFromReservedSlot<dom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	bool bubbles = false;
	dom_exception exc = dom_event_get_bubbles(event, &bubbles);
	args.rval().setBoolean(bubbles);

	return true;
}

static bool
event_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_event *event = JS::GetMaybePtrFromReservedSlot<dom_event>(hobj, 0);

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
event_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct eljs_event *event = JS::GetMaybePtrFromReservedSlot<dom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	args.rval().setBoolean(event->composed);

	return true;
}
#endif

static bool
event_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_event *event = JS::GetMaybePtrFromReservedSlot<dom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	bool prevented = false;
	dom_exception exc = dom_event_is_default_prevented(event, &prevented);
	args.rval().setBoolean(prevented);

	return true;
}

static bool
event_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_event *event = JS::GetMaybePtrFromReservedSlot<dom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	dom_event_prevent_default(event);
	args.rval().setUndefined();

	return true;
}

static bool
event_get_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_event *event = JS::GetMaybePtrFromReservedSlot<dom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	dom_event_target *target = NULL;
	dom_exception exc = dom_event_get_target(event, &target);

	if (exc != DOM_NO_ERR || !target) {
		args.rval().setNull();
		return true;
	}
	JSObject *obj = getElement(ctx, target);
	args.rval().setObject(*obj);
	dom_node_unref(target);

	return true;
}

static bool
event_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_event *event = JS::GetMaybePtrFromReservedSlot<dom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, dom_string_data(typ)));
	dom_string_unref(typ);

	return true;
}

JSObject *
getEvent(JSContext *ctx, void *eve)
{
	JSObject *k = JS_NewObject(ctx, &event_class);

	if (!k) {
		return NULL;
	}
	JS::RootedObject r_event(ctx, k);
	JS_DefineProperties(ctx, r_event, (JSPropertySpec *)event_props);

	dom_event *event = (dom_event *)eve;
	dom_event_ref(event);
	JS::SetReservedSlot(k, 0, JS::PrivateValue(event));

	return k;
}
