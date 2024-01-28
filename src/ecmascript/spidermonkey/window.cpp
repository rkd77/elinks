/* The SpiderMonkey window object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/libdom/dom.h"

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
#include "ecmascript/spidermonkey/css.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/keyboard.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/message.h"
#include "ecmascript/spidermonkey/window.h"
#include "ecmascript/timer.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "main/timer.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
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


static bool window_get_property_closed(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool window_get_property_event(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool window_get_property_innerHeight(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool window_get_property_innerWidth(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool window_get_property_location(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool window_set_property_location(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool window_get_property_parent(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool window_get_property_self(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool window_get_property_status(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool window_set_property_status(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool window_get_property_top(JSContext *ctx, unsigned int argc, JS::Value *vp);

extern struct term_event last_event;

struct listener {
	LIST_HEAD_EL(struct listener);
	char *typ;
	JS::RootedValue fun;
};

struct el_window {
	struct ecmascript_interpreter *interpreter;
	JS::RootedObject thisval;
	LIST_OF(struct listener) listeners;
	JS::RootedValue onmessage;
};

struct el_message {
	JS::RootedObject messageObject;
	struct el_window *elwin;
};

static void
window_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct el_window *elwin = JS::GetMaybePtrFromReservedSlot<struct el_window>(obj, 0);

	if (elwin) {
		struct listener *l;

		foreach(l, elwin->listeners) {
			mem_free_set(&l->typ, NULL);
		}
		free_list(elwin->listeners);
		mem_free(elwin);
	}
}

JSClassOps window_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	window_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass window_class = {
	"window",
	JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_GLOBAL_FLAGS,	/* struct view_state * */
	&window_ops
};

/* "location" is special because we need to simulate "location.href"
 * when the code is asking directly for "location". We do not register
 * it as a "known" property since that was yielding strange bugs
 * (SpiderMonkey was still asking us about the "location" string after
 * assigning to it once), instead we do just a little string
 * comparing. */
JSPropertySpec window_props[] = {
	JS_PSG("closed",	window_get_property_closed, JSPROP_ENUMERATE),
	JS_PSG("event",		window_get_property_event, JSPROP_ENUMERATE),
	JS_PSG("innerHeight",	window_get_property_innerHeight, JSPROP_ENUMERATE),
	JS_PSG("innerWidth",	window_get_property_innerWidth, JSPROP_ENUMERATE),
	JS_PSGS("location",	window_get_property_location, window_set_property_location, JSPROP_ENUMERATE),
	JS_PSG("parent",	window_get_property_parent, JSPROP_ENUMERATE),
	JS_PSG("self",	window_get_property_self, JSPROP_ENUMERATE),
	JS_PSGS("status",	window_get_property_status, window_set_property_status, 0),
	JS_PSG("top",	window_get_property_top, JSPROP_ENUMERATE),
	JS_PSG("window",	window_get_property_self, JSPROP_ENUMERATE),
	JS_PS_END
};


#if 0
static struct frame_desc *
find_child_frame(struct document_view *doc_view, struct frame_desc *tframe)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct frameset_desc *frameset = doc_view->document->frame_desc;
	int i;

	if (!frameset)
		return NULL;

	for (i = 0; i < frameset->n; i++) {
		struct frame_desc *frame = &frameset->frame_desc[i];

		if (frame == tframe)
			return frame;
	}

	return NULL;
}
#endif


void location_goto(struct document_view *doc_view, char *url);

static bool window_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool window_alert(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool window_clearInterval(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool window_clearTimeout(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool window_getComputedStyle(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool window_open(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool window_postMessage(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool window_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool window_setInterval(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool window_setTimeout(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec window_funcs[] = {
	{ "addEventListener", window_addEventListener, 3 },
	{ "alert",	window_alert,		1 },
	{ "clearInterval",	window_clearInterval,	1 },
	{ "clearTimeout",	window_clearTimeout,	1 },
	{ "getComputedStyle",	window_getComputedStyle,	2 },
	{ "open",	window_open,		3 },
	{ "postMessage",	window_postMessage,	3 },
	{ "removeEventListener", window_removeEventListener, 3 },
	{ "setInterval",	window_setInterval,	2 },
	{ "setTimeout",	window_setTimeout,	2 },
	{ NULL }
};

static void
onmessage_run(void *data)
{
	struct el_message *mess = (struct el_message *)data;

	if (mess) {
		struct el_window *elwin = mess->elwin;

		if (!elwin) {
			mem_free(mess);
			return;
		}

		struct ecmascript_interpreter *interpreter = elwin->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());
		JS::RootedValue r_val(ctx);
		interpreter->heartbeat = add_heartbeat(interpreter);

		JS::RootedValueVector argv(ctx);
		if (!argv.resize(1)) {
			return;
		}
		argv[0].setObject(*(mess->messageObject));

		struct listener *l;

		foreach(l, elwin->listeners) {
			if (strcmp(l->typ, "message")) {
				continue;
			}
			JS_CallFunctionValue(ctx, elwin->thisval, l->fun, argv, &r_val);
		}
		JS_CallFunctionValue(ctx, elwin->thisval, elwin->onmessage, argv, &r_val);
		done_heartbeat(interpreter->heartbeat);
		mem_free(mess);
		check_for_rerender(interpreter, "window_onmessage");
	}
}

static bool
window_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct el_window *elwin = JS::GetMaybePtrFromReservedSlot<struct el_window>(hobj, 0);

	if (!elwin) {
		elwin = (struct el_window *)mem_calloc(1, sizeof(*elwin));

		if (!elwin) {
			return false;
		}
		init_list(elwin->listeners);
		elwin->interpreter = interpreter;
		elwin->thisval = hobj;
		JS::SetReservedSlot(hobj, 0, JS::PrivateValue(elwin));
	}

	if (argc < 2) {
		args.rval().setUndefined();
		return true;
	}
	char *method = jsval_to_string(ctx, args[0]);
	JS::RootedValue fun(ctx, args[1]);

	struct listener *l;

	foreach(l, elwin->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (l->fun == fun) {
			args.rval().setUndefined();
			mem_free(method);
			return true;
		}
	}
	struct listener *n = (struct listener *)mem_calloc(1, sizeof(*n));

	if (n) {
		n->typ = method;
		n->fun = fun;
		add_to_list_end(elwin->listeners, n);
	}
	args.rval().setUndefined();
	return true;
}

static bool
window_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct el_window *elwin = JS::GetMaybePtrFromReservedSlot<struct el_window>(hobj, 0);

	if (argc < 2) {
		args.rval().setUndefined();
		return true;
	}
	char *method = jsval_to_string(ctx, args[0]);

	if (!method) {
		return false;
	}
	JS::RootedValue fun(ctx, args[1]);

	struct listener *l;

	foreach(l, elwin->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (l->fun == fun) {
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			mem_free(l);
			mem_free(method);
			args.rval().setUndefined();
			return true;
		}
	}
	mem_free(method);
	args.rval().setUndefined();
	return true;
}

static bool
window_postMessage(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct el_window *elwin = JS::GetMaybePtrFromReservedSlot<struct el_window>(hobj, 0);

	if (argc < 2) {
		args.rval().setUndefined();
		return true;
	}
	char *data = jsval_to_string(ctx, args[0]);
	char *targetOrigin = jsval_to_string(ctx, args[1]);
	char *source = stracpy("TODO");

	JSObject *val = get_messageEvent(ctx, data, targetOrigin, source);

	mem_free_if(data);
	mem_free_if(targetOrigin);
	mem_free_if(source);

	if (!val || !elwin) {
		args.rval().setUndefined();
		return true;
	}
	struct el_message *mess = (struct el_message *)mem_calloc(1, sizeof(*mess));
	if (!mess) {
		return false;
	}
	JS::RootedObject messageObject(ctx, val);
	mess->messageObject = messageObject;
	mess->elwin = elwin;
	register_bottom_half(onmessage_run, mess);
	args.rval().setUndefined();
	return true;
}

/* @window_funcs{"alert"} */
static bool
window_alert(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
//	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

//	JS::Value *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	char *string;

//	if (!JS_InstanceOf(ctx, hobj, &window_class, &args)) {
//		return false;
//	}

	vs = interpreter->vs;

	if (argc != 1)
		return true;

	string = jsval_to_string(ctx, args[0]);

	if (!string)
		return true;

	info_box(vs->doc_view->session->tab->term, MSGBOX_FREE_TEXT,
		N_("JavaScript Alert"), ALIGN_CENTER, string);

	args.rval().setUndefined();
	return true;
}

/* @window_funcs{"open"} */
static bool
window_open(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
//	JS::Value *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	char *frame = NULL;
	char *url, *url2;
	struct uri *uri;
	static time_t ratelimit_start;
	static int ratelimit_count;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &window_class, &args)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	ses = doc_view->session;

	if (get_opt_bool("ecmascript.block_window_opening", ses)) {
#ifdef CONFIG_LEDS
		set_led_value(ses->status.popup_led, 'P');
#endif
		return true;
	}

	if (argc < 1) return true;

	/* Ratelimit window opening. Recursive window.open() is very nice.
	 * We permit at most 20 tabs in 2 seconds. The ratelimiter is very
	 * rough but shall suffice against the usual cases. */
	if (!ratelimit_start || time(NULL) - ratelimit_start > 2) {
		ratelimit_start = time(NULL);
		ratelimit_count = 0;
	} else {
		ratelimit_count++;
		if (ratelimit_count > 20) {
			return true;
		}
	}

	url = jsval_to_string(ctx, args[0]);
	if (!url) {
		return true;
	}
	trim_chars(url, ' ', 0);
	url2 = join_urls(doc_view->document->uri, url);
	mem_free(url);
	if (!url2) {
		return true;
	}
	if (argc > 1) {
		frame = jsval_to_string(ctx, args[1]);
		if (!frame) {
			mem_free(url2);
			return true;
		}
	}

	/* TODO: Support for window naming and perhaps some window features? */

	uri = get_uri(url2, URI_NONE);
	mem_free(url2);
	if (!uri) {
		mem_free_if(frame);
		return true;
	}

	if (frame && *frame && c_strcasecmp(frame, "_blank")) {
		struct delayed_open *deo = (struct delayed_open *)mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			deo->target = stracpy(frame);
			register_bottom_half(delayed_goto_uri_frame, deo);
			args.rval().setBoolean(true);
			goto end;
		}
	}

	if (!get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home")
	    && !get_cmd_opt_bool("anonymous")
	    && can_open_in_new(ses->tab->term)) {
		open_uri_in_new_window(ses, uri, NULL, ENV_ANY,
				       CACHE_MODE_NORMAL, TASK_NONE);
		args.rval().setBoolean(true);
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = (struct delayed_open *)mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half(delayed_open, deo);
			args.rval().setBoolean(true);
		} else {
			args.rval().setUndefined();
		}
	}

end:
	done_uri(uri);
	mem_free_if(frame);

	return true;
}

/* @window_funcs{"setInterval"} */
static bool
window_setInterval(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	char *code;
	int timeout;

	if (argc != 2)
		return true;

	timeout = args[1].toInt32();

	if (timeout <= 0) {
		return true;
	}

	if (args[0].isString()) {
		code = jsval_to_string(ctx, args[0]);

		if (!code) {
			return true;
		}

		struct ecmascript_timeout *id = ecmascript_set_timeout(ctx, code, timeout, timeout);
		JS::BigInt *bi = JS::NumberToBigInt(ctx, reinterpret_cast<int64_t>(id));
		args.rval().setBigInt(bi);
		return true;
	}
	struct ecmascript_timeout *id = ecmascript_set_timeout2(ctx, args[0], timeout, timeout);
	JS::BigInt *bi = JS::NumberToBigInt(ctx, reinterpret_cast<int64_t>(id));
	args.rval().setBigInt(bi);

	return true;
}

/* @window_funcs{"setTimeout"} */
static bool
window_setTimeout(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	char *code;
	int timeout;

	if (argc != 2)
		return true;

	timeout = args[1].toInt32();

	if (timeout <= 0) {
		return true;
	}

	if (args[0].isString()) {
		code = jsval_to_string(ctx, args[0]);

		if (!code) {
			return true;
		}

		struct ecmascript_timeout *id = ecmascript_set_timeout(ctx, code, timeout, -1);
		JS::BigInt *bi = JS::NumberToBigInt(ctx, reinterpret_cast<int64_t>(id));
		args.rval().setBigInt(bi);
		return true;
	}
	struct ecmascript_timeout *id = ecmascript_set_timeout2(ctx, args[0], timeout, -1);
	JS::BigInt *bi = JS::NumberToBigInt(ctx, reinterpret_cast<int64_t>(id));
	args.rval().setBigInt(bi);

	return true;
}

/* @window_funcs{"clearInterval"} */
static bool
window_clearInterval(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

	if (argc != 1) {
		return true;
	}
	JS::BigInt *bi = JS::ToBigInt(ctx, args[0]);
	int64_t number = JS::ToBigInt64(bi);
	struct ecmascript_timeout *t = reinterpret_cast<struct ecmascript_timeout *>(number);

	if (t && found_in_map_timer(t->tid)) {
		kill_timer(&t->tid);
		done_string(&t->code);
		del_from_list(t);
		mem_free(t);
	}
	return true;
}


/* @window_funcs{"clearTimeout"} */
static bool
window_clearTimeout(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

	if (argc != 1) {
		return true;
	}
	JS::BigInt *bi = JS::ToBigInt(ctx, args[0]);
	int64_t number = JS::ToBigInt64(bi);
	struct ecmascript_timeout *t = reinterpret_cast<struct ecmascript_timeout *>(number);

	if (t && found_in_map_timer(t->tid)) {
		kill_timer(&t->tid);
		done_string(&t->code);
		del_from_list(t);
		mem_free(t);
	}
	return true;
}

/* @window_funcs{"getComputedStyle"} */
static bool
window_getComputedStyle(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	if (argc < 1) {
		return true;
	}

	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject node(ctx, &args[0].toObject());
	dom_node *el = (dom_node *)JS::GetMaybePtrFromReservedSlot<dom_node>(node, 0);
	JSObject *css = getCSSStyleDeclaration(ctx, el);
	args.rval().setObject(*css);

	return true;
}


#if 0
static bool
window_get_property_closed(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	ELINKS_CAST_PROP_PARAMS

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &window_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	boolean_to_jsval(ctx, vp, 0);

	return true;
}
#endif

static bool
window_get_property_closed(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setBoolean(false);

	return true;
}

static bool
window_get_property_event(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JSObject *event = get_keyboardEvent(ctx, &last_event);
	args.rval().setObject(*event);

	return true;
}

static bool
window_get_property_innerHeight(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &window_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setInt32(doc_view->box.height * ses->tab->term->cell_height);

	return true;
}

static bool
window_get_property_innerWidth(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &window_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setInt32(doc_view->box.width * ses->tab->term->cell_width);

	return true;
}

static bool
window_get_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!interpreter->location_obj) {
		interpreter->location_obj = getLocation(ctx);
	}
	args.rval().setObject(*(JSObject *)(interpreter->location_obj));
	return true;
}

static bool
window_set_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	struct view_state *vs;
	struct document_view *doc_view;

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	char *url = jsval_to_string(ctx, args[0]);

	if (url) {
		location_goto(doc_view, url);
		mem_free(url);
	}

	return true;
}

static bool
window_get_property_parent(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	/* XXX: It would be nice if the following worked, yes.
	 * The problem is that we get called at the point where
	 * document.frame properties are going to be mostly NULL.
	 * But the problem is deeper because at that time we are
	 * yet building scrn_frames so our parent might not be there
	 * yet (XXX: is this true?). The true solution will be to just
	 * have struct document_view *(document_view.parent). --pasky */
	/* FIXME: So now we alias window.parent to window.top, which is
	 * INCORRECT but works for the most common cases of just two
	 * frames. Better something than nothing. */
	args.rval().setUndefined();

	return true;
}

static bool
window_get_property_self(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	args.rval().setObject(*hobj);

	return true;
}

static bool
window_get_property_status(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setUndefined();

	return true;
}

static bool
window_set_property_status(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	if (args.length() != 1) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
		return true;
	}

	mem_free_set(&vs->doc_view->session->status.window_status, jsval_to_string(ctx, args[0]));
	print_screen_status(vs->doc_view->session);

	return true;
}

static bool
window_get_property_top(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document_view *top_view;
	JSObject *newjsframe;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	top_view = doc_view->session->doc_view;

	assert(top_view && top_view->vs);
	if (top_view->vs->ecmascript_fragile)
		ecmascript_reset_state(top_view->vs);
	if (!top_view->vs->ecmascript) {
		args.rval().setUndefined();
		return true;
	}
	newjsframe = JS::CurrentGlobalOrNull((JSContext *)top_view->vs->ecmascript->backend_data);

	/* Keep this unrolled this way. Will have to check document.domain
	 * JS property. */
	/* Note that this check is perhaps overparanoid. If top windows
	 * is alien but some other child window is not, we should still
	 * let the script walk thru. That'd mean moving the check to
	 * other individual properties in this switch. */
	if (compare_uri(vs->uri, top_view->vs->uri, URI_HOST)) {
		args.rval().setObject(*newjsframe);
		return true;
	}
		/* else */
		/****X*X*X*** SECURITY VIOLATION! RED ALERT, SHIELDS UP! ***X*X*X****\
		|* (Pasky was apparently looking at the Links2 JS code   .  ___ ^.^ *|
		\* for too long.)                                        `.(,_,)\o/ */
	args.rval().setUndefined();
	return true;
}
