/* The SpiderMonkey CustomEvent object implementation. */

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
#include "ecmascript/spidermonkey/customevent.h"
#include "ecmascript/spidermonkey/heartbeat.h"
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

struct eljscustom_event {
	JS::RootedObject detail;
	char *type_;
	unsigned int bubbles:1;
	unsigned int cancelable:1;
	unsigned int composed:1;
	unsigned int defaultPrevented:1;
};

static bool customEvent_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_detail(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp);

static bool customEvent_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp);

static void
customEvent_finalize(JS::GCContext *op, JSObject *customEvent_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	struct eljscustom_event *event = JS::GetMaybePtrFromReservedSlot<struct eljscustom_event>(customEvent_obj, 0);

	if (event) {
		mem_free_if(event->type_);
		mem_free(event);
	}
}

JSClassOps customEvent_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	customEvent_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass customEvent_class = {
	"CustomEvent",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&customEvent_ops
};

bool
customEvent_constructor(JSContext* ctx, unsigned argc, JS::Value* vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject newObj(ctx, JS_NewObjectForConstructor(ctx, &customEvent_class, args));
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
	struct eljscustom_event *event = (struct eljscustom_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		return false;
	}
	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(event));

	if (argc > 0) {
		event->type_ = jsval_to_string(ctx, args[0]);
	}

	if (argc > 1) {
		JS::RootedValue v(ctx);
		JS::RootedObject v_obj(ctx, &args[1].toObject());

		if (JS_GetProperty(ctx, v_obj, "bubbles", &v)) {
			event->bubbles = (unsigned int)v.toBoolean();
		}
		if (JS_GetProperty(ctx, v_obj, "cancelable", &v)) {
			event->cancelable = (unsigned int)v.toBoolean();
		}
		if (JS_GetProperty(ctx, v_obj, "composed", &v)) {
			event->composed = (unsigned int)v.toBoolean();
		}
		if (JS_GetProperty(ctx, v_obj, "detail", &v)) {
			JS::RootedObject vv(ctx, &v.toObject());
			event->detail = vv;
		}
	}
	args.rval().setObject(*newObj);

	return true;
}

JSPropertySpec customEvent_props[] = {
	JS_PSG("bubbles",	customEvent_get_property_bubbles, JSPROP_ENUMERATE),
	JS_PSG("cancelable",	customEvent_get_property_cancelable, JSPROP_ENUMERATE),
	JS_PSG("composed",	customEvent_get_property_composed, JSPROP_ENUMERATE),
	JS_PSG("defaultPrevented",	customEvent_get_property_defaultPrevented, JSPROP_ENUMERATE),
	JS_PSG("detail", customEvent_get_property_detail, JSPROP_ENUMERATE),
	JS_PSG("type",	customEvent_get_property_type, JSPROP_ENUMERATE),
	JS_PS_END
};

const spidermonkeyFunctionSpec customEvent_funcs[] = {
	{ "preventDefault",	customEvent_preventDefault,	0 },
	{ NULL }
};

static bool
customEvent_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct eljscustom_event *event = JS::GetMaybePtrFromReservedSlot<struct eljscustom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	args.rval().setBoolean(event->bubbles);

	return true;
}

static bool
customEvent_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct eljscustom_event *event = JS::GetMaybePtrFromReservedSlot<struct eljscustom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	args.rval().setBoolean(event->cancelable);

	return true;
}

static bool
customEvent_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct eljscustom_event *event = JS::GetMaybePtrFromReservedSlot<struct eljscustom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	args.rval().setBoolean(event->composed);

	return true;
}

static bool
customEvent_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct eljscustom_event *event = JS::GetMaybePtrFromReservedSlot<struct eljscustom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	args.rval().setBoolean(event->defaultPrevented);

	return true;
}

static bool
customEvent_get_property_detail(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct eljscustom_event *event = JS::GetMaybePtrFromReservedSlot<struct eljscustom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	if (!event->detail) {
		args.rval().setNull();
		return true;
	}
	args.rval().setObject(*event->detail);

	return true;
}

static bool
customEvent_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct eljscustom_event *event = JS::GetMaybePtrFromReservedSlot<struct eljscustom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	if (event->cancelable) {
		event->defaultPrevented = 1;
	}
	args.rval().setUndefined();

	return true;
}

static bool
customEvent_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct eljscustom_event *event = JS::GetMaybePtrFromReservedSlot<struct eljscustom_event>(hobj, 0);

	if (!event) {
		return false;
	}

	if (!event->type_) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, event->type_));

	return true;
}

JSObject *
get_customEvent(JSContext *ctx)
{
	JSObject *k = JS_NewObject(ctx, &customEvent_class);

	if (!k) {
		return NULL;
	}
	JS::RootedObject r_event(ctx, k);
	JS_DefineProperties(ctx, r_event, (JSPropertySpec *)customEvent_props);

	struct eljscustom_event *event = (struct eljscustom_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		return NULL;
	}
	JS::SetReservedSlot(k, 0, JS::PrivateValue(event));

	return k;
}
