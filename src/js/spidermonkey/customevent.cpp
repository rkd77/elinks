/* The SpiderMonkey CustomEvent object implementation. */

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
#include "js/libdom/dom.h"
#include "js/spidermonkey.h"
#include "js/spidermonkey/customevent.h"
#include "js/spidermonkey/element.h"
#include "js/spidermonkey/heartbeat.h"
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

static bool customEvent_get_property_bubbles(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_cancelable(JSContext *ctx, unsigned int argc, JS::Value *vp);
//static bool customEvent_get_property_composed(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_defaultPrevented(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_detail(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool customEvent_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp);

static bool customEvent_preventDefault(JSContext *ctx, unsigned int argc, JS::Value *vp);

static void
customEvent_finalize(JS::GCContext *op, JSObject *customEvent_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_custom_event *event = JS::GetMaybePtrFromReservedSlot<dom_custom_event>(customEvent_obj, 0);

	if (event) {
		dom_event_unref(event);
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		return false;
	}
	dom_custom_event *event = NULL;
	dom_string *CustomEventStr = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)"CustomEvent", sizeof("CustomEvent") - 1, &CustomEventStr);

	if (exc != DOM_NO_ERR || !CustomEventStr) {
		return false;
	}
	exc = dom_document_event_create_event(document->dom, CustomEventStr, &event);
	dom_string_unref(CustomEventStr);

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
	JSObject *detail = NULL;

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
		if (JS_GetProperty(ctx, v_obj, "detail", &v)) {
			JS::RootedObject vv(ctx, &v.toObject());
			detail = vv;
		}
	}
	exc = dom_custom_event_init_ns(event, NULL, typ, bubbles, cancelable, detail);
	if (typ) {
		dom_string_unref(typ);
	}
	args.rval().setObject(*newObj);

	return true;
}

JSPropertySpec customEvent_props[] = {
	JS_PSG("bubbles",	customEvent_get_property_bubbles, JSPROP_ENUMERATE),
	JS_PSG("cancelable",	customEvent_get_property_cancelable, JSPROP_ENUMERATE),
//	JS_PSG("composed",	customEvent_get_property_composed, JSPROP_ENUMERATE),
	JS_PSG("defaultPrevented",	customEvent_get_property_defaultPrevented, JSPROP_ENUMERATE),
	JS_PSG("detail", customEvent_get_property_detail, JSPROP_ENUMERATE),
	JS_PSG("target",	customEvent_get_property_target, JSPROP_ENUMERATE),
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
	dom_custom_event *event = JS::GetMaybePtrFromReservedSlot<dom_custom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	bool bubbles = false;
	(void)dom_event_get_bubbles(event, &bubbles);
	args.rval().setBoolean(bubbles);

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
	dom_custom_event *event = JS::GetMaybePtrFromReservedSlot<dom_custom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	bool cancelable = false;
	(void)dom_event_get_cancelable(event, &cancelable);
	args.rval().setBoolean(cancelable);

	return true;
}

#if 0
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
#endif

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
	dom_custom_event *event = JS::GetMaybePtrFromReservedSlot<dom_custom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	bool prevented = false;
	(void)dom_event_is_default_prevented(event, &prevented);
	args.rval().setBoolean(prevented);

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
	dom_custom_event *event = JS::GetMaybePtrFromReservedSlot<dom_custom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	void *detail = NULL;
	dom_exception exc = dom_custom_event_get_detail(event, &detail);

	if (exc != DOM_NO_ERR || !detail) {
		return true;
	}
	args.rval().setObject(*(JSObject *)detail);

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
	dom_custom_event *event = JS::GetMaybePtrFromReservedSlot<dom_custom_event>(hobj, 0);

	if (!event) {
		return false;
	}
	dom_event_prevent_default(event);
	args.rval().setUndefined();

	return true;
}

static bool
customEvent_get_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	dom_custom_event *event = JS::GetMaybePtrFromReservedSlot<dom_custom_event>(hobj, 0);

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
	dom_custom_event *event = JS::GetMaybePtrFromReservedSlot<dom_custom_event>(hobj, 0);

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
get_customEvent(JSContext *ctx)
{
	JSObject *k = JS_NewObject(ctx, &customEvent_class);

	if (!k) {
		return NULL;
	}
	JS::RootedObject r_event(ctx, k);
	JS_DefineProperties(ctx, r_event, (JSPropertySpec *)customEvent_props);

	dom_custom_event *event = NULL;
	dom_exception exc = dom_event_create(&event);

	if (exc != DOM_NO_ERR) {
		return NULL;
	}
	JS::SetReservedSlot(k, 0, JS::PrivateValue(event));

	return k;
}
