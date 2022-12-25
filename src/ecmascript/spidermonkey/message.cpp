/* The SpiderMonkey MessageEvent object implementation. */

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
#include "ecmascript/spidermonkey/message.h"
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


static bool messageEvent_get_property_data(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_lastEventId(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_origin(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool messageEvent_get_property_source(JSContext *cx, unsigned int argc, JS::Value *vp);

struct message_event {
	char *data;
	char *lastEventId;
	char *origin;
	char *source;
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!newObj) {
		return false;
	}
	struct message_event *event = (struct message_event *)mem_calloc(1, sizeof(*event));

	if (!event) {
		return false;
	}
	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(event));
	args.rval().setObject(*newObj);

	return true;
}

JSPropertySpec messageEvent_props[] = {
	JS_PSG("data",	messageEvent_get_property_data, JSPROP_ENUMERATE),
	JS_PSG("lastEventId",	messageEvent_get_property_lastEventId, JSPROP_ENUMERATE),
	JS_PSG("origin",	messageEvent_get_property_origin, JSPROP_ENUMERATE),
	JS_PSG("source",	messageEvent_get_property_source, JSPROP_ENUMERATE),
	JS_PS_END
};

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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
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

	snprintf(id, "%d", 31, ++lastEventId);
	event->lastEventId = stracpy(id);
	JS::SetReservedSlot(e, 0, JS::PrivateValue(event));

	return e;
}
