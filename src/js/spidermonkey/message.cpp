/* The SpiderMonkey MessageEvent object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/spidermonkey/util.h"
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
#include "js/ecmascript.h"
#include "js/spidermonkey.h"
#include "js/spidermonkey/heartbeat.h"
#include "js/spidermonkey/message.h"
#include "js/timer.h"
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


static bool messageEvent_get_property_data(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_lastEventId(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_origin(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_source(JSContext *cx, unsigned int argc, JS::Value *vp);

static bool messageEvent_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp);

static bool messageEvent_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp);


struct message_event {
	char *data;
	char *lastEventId;
	char *origin;
	char *source;

	char *type_;
	unsigned int bubbles:1;
	unsigned int cancelable:1;
	unsigned int composed:1;
	unsigned int defaultPrevented:1;
};

static void
messageEvent_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(obj, 0);

	if (event) {
		mem_free_if(event->data);
		mem_free_if(event->lastEventId);
		mem_free_if(event->origin);
		mem_free_if(event->source);
		mem_free_if(event->type_);
		mem_free(event);
	}
}

JSClassOps messageEvent_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	messageEvent_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass messageEvent_class = {
	"MessageEvent",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&messageEvent_ops
};

bool
messageEvent_constructor(JSContext* ctx, unsigned argc, JS::Value* vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject newObj(ctx, JS_NewObjectForConstructor(ctx, &messageEvent_class, args));
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
	struct message_event *event = (struct message_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		return false;
	}

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
		if (JS_GetProperty(ctx, v_obj, "data", &v)) {
			event->data = jsval_to_string(ctx, v);
		}
		if (JS_GetProperty(ctx, v_obj, "lastEventId", &v)) {
			event->lastEventId = jsval_to_string(ctx, v);
		}
		if (JS_GetProperty(ctx, v_obj, "origin", &v)) {
			event->origin = jsval_to_string(ctx, v);
		}
		if (JS_GetProperty(ctx, v_obj, "source", &v)) {
			event->source = jsval_to_string(ctx, v);
		}
	}
	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(event));
	args.rval().setObject(*newObj);

	return true;
}

JSPropertySpec messageEvent_props[] = {
	JS_PSG("bubbles",	messageEvent_get_property_bubbles, JSPROP_ENUMERATE),
	JS_PSG("cancelable",	messageEvent_get_property_cancelable, JSPROP_ENUMERATE),
	JS_PSG("composed",	messageEvent_get_property_composed, JSPROP_ENUMERATE),
	JS_PSG("data",	messageEvent_get_property_data, JSPROP_ENUMERATE),
	JS_PSG("defaultPrevented",	messageEvent_get_property_defaultPrevented, JSPROP_ENUMERATE),
	JS_PSG("lastEventId",	messageEvent_get_property_lastEventId, JSPROP_ENUMERATE),
	JS_PSG("origin",	messageEvent_get_property_origin, JSPROP_ENUMERATE),
	JS_PSG("source",	messageEvent_get_property_source, JSPROP_ENUMERATE),
	JS_PSG("type",	messageEvent_get_property_type, JSPROP_ENUMERATE),
	JS_PS_END
};

const spidermonkeyFunctionSpec messageEvent_funcs[] = {
	{ "preventDefault",	messageEvent_preventDefault,	0 },
	{ NULL }
};

static bool
messageEvent_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

	if (!event) {
		return false;
	}
	args.rval().setBoolean(event->bubbles);

	return true;
}

static bool
messageEvent_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

	if (!event) {
		return false;
	}
	args.rval().setBoolean(event->cancelable);

	return true;
}

static bool
messageEvent_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

	if (!event) {
		return false;
	}
	args.rval().setBoolean(event->composed);

	return true;
}

static bool
messageEvent_get_property_data(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

	if (!event) {
		return false;
	}

	if (!event->data) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, event->data));

	return true;
}

static bool
messageEvent_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

	if (!event) {
		return false;
	}
	args.rval().setBoolean(event->defaultPrevented);

	return true;
}

static bool
messageEvent_get_property_lastEventId(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

	if (!event) {
		return false;
	}

	if (!event->lastEventId) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, event->lastEventId));

	return true;
}

static bool
messageEvent_get_property_origin(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

	if (!event) {
		return false;
	}

	if (!event->origin) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, event->origin));

	return true;
}

static bool
messageEvent_get_property_source(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

	if (!event) {
		return false;
	}

// TODO proper type
	if (!event->source) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, event->source));

	return true;
}

static bool
messageEvent_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

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
messageEvent_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	struct message_event *event = JS::GetMaybePtrFromReservedSlot<struct message_event>(hobj, 0);

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

static int lastEventId;

JSObject *
get_messageEvent(JSContext *ctx, char *data, char *origin, char *source)
{
	JSObject *e = JS_NewObject(ctx, &messageEvent_class);

	if (!e) {
		return NULL;
	}

	JS::RootedObject r_event(ctx, e);
	JS_DefineProperties(ctx, r_event, (JSPropertySpec *) messageEvent_props);

	struct message_event *event = (struct message_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		return NULL;
	}
	event->data = null_or_stracpy(data);
	event->origin = null_or_stracpy(origin);
	event->source = null_or_stracpy(source);

	char id[32];

	snprintf(id, 31, "%d", ++lastEventId);
	event->lastEventId = stracpy(id);
	JS::SetReservedSlot(e, 0, JS::PrivateValue(event));

	return e;
}
