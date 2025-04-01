/* The Quickjs window object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "bfu/leds.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/quickjs.h"
#include "js/quickjs/css.h"
#include "js/quickjs/element.h"
#include "js/quickjs/heartbeat.h"
#include "js/quickjs/keyboard.h"
#include "js/quickjs/location.h"
#include "js/quickjs/message.h"
#include "js/quickjs/window.h"
#include "js/timer.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "util/conv.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_window_class_id;

struct win_listener {
	LIST_HEAD_EL(struct win_listener);
	char *typ;
	JSValue fun;
};

struct el_window {
	struct ecmascript_interpreter *interpreter;
	JSValue thisval;
	LIST_OF(struct win_listener) listeners;
	JSValue onmessage;
	struct view_state *vs;
};

struct el_message {
	JSValue messageObject;
	struct el_window *elwin;
};

extern struct term_event last_event;

static void
js_window_finalize(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct el_window *elwin = (struct el_window *)JS_GetOpaque(val, js_window_class_id);

	if (elwin) {
		struct win_listener *l;

		foreach(l, elwin->listeners) {
			mem_free_set(&l->typ, NULL);
		}
		free_list(elwin->listeners);

		if (elwin->vs) {
			elwin->vs->win_obj = JS_NULL;
		}
		mem_free(elwin);
	}
}

void
detach_js_view_state(struct view_state *vs)
{
	assert(vs);
	if_assert_failed return;

	if (!JS_IsNull(vs->win_obj)) {
		struct el_window *elwin = (struct el_window *)JS_GetOpaque(vs->win_obj, js_window_class_id);

		if (elwin) {
			elwin->vs = NULL;
		}
		vs->win_obj = JS_NULL;
	}

	if (!JS_IsNull(vs->location_obj)) {
		JS_SetOpaque(vs->location_obj, NULL);
		vs->location_obj = JS_NULL;
	}
}

static void
js_window_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct el_window *elwin = (struct el_window *)JS_GetOpaque(val, js_window_class_id);

	if (elwin) {
		JS_MarkValue(rt, elwin->thisval, mark_func);
		JS_MarkValue(rt, elwin->onmessage, mark_func);

		if (elwin->interpreter) {
			struct ecmascript_timeout *et;

			foreach (et, elwin->interpreter->timeouts) {
				JS_MarkValue(rt, et->fun, mark_func);
			}
		}

		struct win_listener *l;

		foreach (l, elwin->listeners) {
			JS_MarkValue(rt, l->fun, mark_func);
		}
	}
}

/* @window_funcs{"open"} */
JSValue
js_window_open(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	const char *frame = NULL;
	char *url, *url2;
	struct uri *uri;
	static time_t ratelimit_start;
	static int ratelimit_count;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	ses = doc_view->session;

	if (get_opt_bool("ecmascript.block_window_opening", ses)) {
#ifdef CONFIG_LEDS
		set_led_value(ses->status.popup_led, 'P');
#endif
		return JS_UNDEFINED;
	}

	if (argc < 1) return JS_UNDEFINED;

	/* Ratelimit window opening. Recursive window.open() is very nice.
	 * We permit at most 20 tabs in 2 seconds. The ratelimiter is very
	 * rough but shall suffice against the usual cases. */
	if (!ratelimit_start || time(NULL) - ratelimit_start > 2) {
		ratelimit_start = time(NULL);
		ratelimit_count = 0;
	} else {
		ratelimit_count++;
		if (ratelimit_count > 20) {
			return JS_UNDEFINED;
		}
	}

	const char *str;
	size_t len;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}

	url = stracpy(str);
	JS_FreeCString(ctx, str);

	if (!url) {
		return JS_UNDEFINED;
	}
	trim_chars(url, ' ', 0);
	url2 = join_urls(doc_view->document->uri, url);
	mem_free(url);

	if (!url2) {
		return JS_UNDEFINED;
	}

	if (argc > 1) {
		size_t len2;
		frame = JS_ToCStringLen(ctx, &len2, argv[1]);

		if (!frame) {
			mem_free(url2);
			return JS_EXCEPTION;
		}
	}

	/* TODO: Support for window naming and perhaps some window features? */

	uri = get_uri(url2, URI_NONE);
	mem_free(url2);
	if (!uri) {
		if (frame) JS_FreeCString(ctx, frame);
		return JS_UNDEFINED;
	}

	JSValue ret;

	if (frame && *frame && c_strcasecmp(frame, "_blank")) {
		struct delayed_open *deo = (struct delayed_open *)mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			deo->target = stracpy(frame);
			register_bottom_half(delayed_goto_uri_frame, deo);
			ret = JS_TRUE;
			goto end;
		}
	}

	if (!get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home")
	    && !get_cmd_opt_bool("anonymous")
	    && can_open_in_new(ses->tab->term)) {
		open_uri_in_new_window(ses, uri, NULL, ENV_ANY,
				       CACHE_MODE_NORMAL, TASK_NONE);
		ret = JS_TRUE;
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = (struct delayed_open *)mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half(delayed_open, deo);
			ret = JS_TRUE;
		} else {
			ret = JS_UNDEFINED;
		}
	}

end:
	done_uri(uri);
	if (frame) JS_FreeCString(ctx, frame);

	return ret;
}

/* @window_funcs{"setInterval"} */
JSValue
js_window_setInterval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	int timeout = 0;
	JSValueConst func;

	if (argc != 2) {
		return JS_UNDEFINED;
	}

	if (JS_ToInt32(ctx, &timeout, argv[1])) {
		return JS_EXCEPTION;
	}

	if (timeout <= 0) {
		return JS_UNDEFINED;
	}

	func = argv[0];

	if (JS_IsFunction(ctx, func)) {
		struct ecmascript_timeout *id = ecmascript_set_timeout2q(ctx, func, timeout, timeout);

#if SIZEOF_INTPTR_T == 4
		return JS_NewInt32(ctx, (intptr_t)(id));
#else
		return JS_NewInt64(ctx, (intptr_t)(id));
#endif
	}

	if (JS_IsString(func)) {
		const char *code = JS_ToCString(ctx, func);

		if (!code) {
			return JS_EXCEPTION;
		}
		char *code2 = stracpy(code);
		JS_FreeCString(ctx, code);

		if (code2) {
			struct ecmascript_timeout *id = ecmascript_set_timeout(ctx, code2, timeout, timeout);

#if SIZEOF_INTPTR_T == 4
			return JS_NewInt32(ctx, (intptr_t)(id));
#else
			return JS_NewInt64(ctx, (intptr_t)(id));
#endif
		}
	}

	return JS_UNDEFINED;
}


/* @window_funcs{"setTimeout"} */
JSValue
js_window_setTimeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	int timeout = 0;
	JSValueConst func;

	if (argc < 1) {
		return JS_UNDEFINED;
	}

	if (argc > 1) {
		if (JS_ToInt32(ctx, &timeout, argv[1])) {
			return JS_EXCEPTION;
		}
	}

	if (timeout < 0) {
		return JS_UNDEFINED;
	}

	func = argv[0];

	if (JS_IsFunction(ctx, func)) {
		struct ecmascript_timeout *id = ecmascript_set_timeout2q(ctx, func, timeout, 0);
#if SIZEOF_INTPTR_T == 4
		return JS_NewInt32(ctx, (intptr_t)(id));
#else
		return JS_NewInt64(ctx, (intptr_t)(id));
#endif
	}

	if (JS_IsString(func)) {
		const char *code = JS_ToCString(ctx, func);

		if (!code) {
			return JS_EXCEPTION;
		}
		char *code2 = stracpy(code);
		JS_FreeCString(ctx, code);

		if (code2) {
			struct ecmascript_timeout *id = ecmascript_set_timeout(ctx, code2, timeout, 0);
#if SIZEOF_INTPTR_T == 4
			return JS_NewInt32(ctx, (intptr_t)(id));
#else
			return JS_NewInt64(ctx, (intptr_t)(id));
#endif
		}
	}

	return JS_UNDEFINED;
}

/* @window_funcs{"clearInterval"} */
JSValue
js_window_clearInterval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1 || JS_IsNull(argv[0])) {
		return JS_UNDEFINED;
	}
	intptr_t number;

#if SIZEOF_INTPTR_T == 4
	if (JS_ToInt32(ctx, &number, argv[0])) {
		return JS_UNDEFINED;
	}
#else
	if (JS_ToInt64(ctx, &number, argv[0])) {
		return JS_UNDEFINED;
	}
#endif
	struct ecmascript_timeout *t = (struct ecmascript_timeout *)(number);

	if (found_in_map_timer(t)) {
		t->timeout_next = -1;
	}

	return JS_UNDEFINED;
}


/* @window_funcs{"clearTimeout"} */
JSValue
js_window_clearTimeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1 || JS_IsNull(argv[0])) {
		return JS_UNDEFINED;
	}
	intptr_t number;

#if SIZEOF_INTPTR_T == 4
	if (JS_ToInt32(ctx, &number, argv[0])) {
		return JS_UNDEFINED;
	}
#else
	if (JS_ToInt64(ctx, &number, argv[0])) {
		return JS_UNDEFINED;
	}
#endif
	struct ecmascript_timeout *t = (struct ecmascript_timeout *)(number);

	if (found_in_map_timer(t)) {
		t->timeout_next = -1;
	}

	return JS_UNDEFINED;
}

/* @window_funcs{"getComputedStyle"} */
JSValue
js_window_getComputedStyle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc < 1) {
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(js_getopaque(argv[0], js_element_class_id));

	if (!el) {
		return JS_NULL;
	}
	return getCSSStyleDeclaration(ctx, el);
}

static JSValue
js_window_get_property_closed(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_FALSE;
}

static JSValue
js_window_get_property_event(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return get_keyboardEvent(ctx, &last_event);
}

static JSValue
js_window_get_property_innerHeight(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}

	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}

	return JS_NewInt32(ctx, doc_view->box.height * ses->tab->term->cell_height);
}

static JSValue
js_window_get_property_innerWidth(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		return JS_UNDEFINED;
	}

	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}

	return JS_NewInt32(ctx, doc_view->box.width * ses->tab->term->cell_width);
}

static JSValue
js_window_get_property_location(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct el_window *elwin = (struct el_window *)JS_GetOpaque(this_val, js_window_class_id);

	if (elwin && elwin->vs) {
		return getLocation(ctx, elwin->vs);
	}
	JSValue ret = interpreter->location_obj;

	JS_DupValue(ctx, ret);

	RETURN_JS(ret);
}

static JSValue
js_window_set_property_location(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs;
	struct document_view *doc_view;
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	const char *url;
	size_t len;

	url = JS_ToCStringLen(ctx, &len, val);

	if (!url) {
		return JS_EXCEPTION;
	}

	location_goto_const(doc_view, url, 0);
	JS_FreeCString(ctx, url);

	return JS_UNDEFINED;
}


static JSValue
js_window_get_property_parent(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

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

	return JS_UNDEFINED;
}

static JSValue
js_window_get_property_self(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	JSValue r = JS_DupValue(ctx, this_val);

	RETURN_JS(r);
}

static JSValue
js_window_get_property_status(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_UNDEFINED;
}

static JSValue
js_window_set_property_status(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
		return JS_UNDEFINED;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *text = stracpy(str);
	JS_FreeCString(ctx, str);

	mem_free_set(&vs->doc_view->session->status.window_status, text);
	print_screen_status(vs->doc_view->session);

	return JS_UNDEFINED;
}

static JSValue
js_window_get_property_top(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct document_view *doc_view;
	struct document_view *top_view;
	JSValue newjsframe;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	top_view = doc_view->session->doc_view;

	assert(top_view && top_view->vs);
	if (top_view->vs->ecmascript_fragile)
		ecmascript_reset_state(top_view->vs);
	if (!top_view->vs->ecmascript) {
		return JS_UNDEFINED;
	}
	newjsframe = JS_GetGlobalObject((JSContext *)top_view->vs->ecmascript->backend_data);

	/* Keep this unrolled this way. Will have to check document.domain
	 * JS property. */
	/* Note that this check is perhaps overparanoid. If top windows
	 * is alien but some other child window is not, we should still
	 * let the script walk thru. That'd mean moving the check to
	 * other individual properties in this switch. */
	if (compare_uri(vs->uri, top_view->vs->uri, URI_HOST)) {
		return newjsframe;
	}
		/* else */
		/****X*X*X*** SECURITY VIOLATION! RED ALERT, SHIELDS UP! ***X*X*X****\
		|* (Pasky was apparently looking at the Links2 JS code   .  ___ ^.^ *|
		\* for too long.)                                        `.(,_,)\o/ */
	return JS_UNDEFINED;
}

JSValue
js_window_alert(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

	assert(interpreter);
	struct view_state *vs;
	const char *str;
	char *string;
	size_t len;

	vs = interpreter->vs;

	if (argc != 1)
		return JS_UNDEFINED;

	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}

	string = stracpy(str);
	JS_FreeCString(ctx, str);

	info_box(vs->doc_view->session->tab->term, MSGBOX_FREE_TEXT,
		N_("JavaScript Alert"), ALIGN_CENTER, string);

	return JS_UNDEFINED;
}

static JSValue
js_window_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[window object]");
}

static JSValue
js_window_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct el_window *elwin = (struct el_window *)(JS_GetOpaque(this_val, js_window_class_id));

	if (!elwin) {
		elwin = (struct el_window *)mem_calloc(1, sizeof(*elwin));

		if (!elwin) {
			return JS_EXCEPTION;
		}
		init_list(elwin->listeners);
		elwin->interpreter = interpreter;
		elwin->thisval = JS_DupValue(ctx, this_val);
		JS_SetOpaque(this_val, elwin);
	}

	if (argc < 2) {
		return JS_UNDEFINED;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *method = stracpy(str);
	JS_FreeCString(ctx, str);

	if (!method) {
		return JS_EXCEPTION;
	}

	JSValue fun = argv[1];
	struct win_listener *l;

	foreach(l, elwin->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			mem_free(method);
			return JS_UNDEFINED;
		}
	}
	struct win_listener *n = (struct win_listener *)mem_calloc(1, sizeof(*n));

	if (n) {
		n->typ = method;
		n->fun = JS_DupValue(ctx, argv[1]);
		add_to_list_end(elwin->listeners, n);
	}
	return JS_UNDEFINED;
}

static JSValue
js_window_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct el_window *elwin = (struct el_window *)(JS_GetOpaque(this_val, js_window_class_id));

	if (!elwin) {
		return JS_NULL;
	}

	if (argc < 2) {
		return JS_UNDEFINED;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *method = stracpy(str);
	JS_FreeCString(ctx, str);

	if (!method) {
		return JS_EXCEPTION;
	}
	JSValue fun = argv[1];
	struct win_listener *l;

	foreach(l, elwin->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			mem_free(l);
			mem_free(method);
			return JS_UNDEFINED;
		}
	}
	mem_free(method);
	return JS_UNDEFINED;
}

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
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct win_listener *l;

		foreach(l, elwin->listeners) {
			if (strcmp(l->typ, "message")) {
				continue;
			}
			JSValue func = JS_DupValue(ctx, l->fun);
			JSValue arg = JS_DupValue(ctx, mess->messageObject);
			JSValue ret = JS_Call(ctx, func, elwin->thisval, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		if (JS_IsFunction(ctx, elwin->onmessage)) {
			JSValue func = JS_DupValue(ctx, elwin->onmessage);
			JSValue arg = JS_DupValue(ctx, mess->messageObject);
			JSValue ret = JS_Call(ctx, func, elwin->thisval, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		done_heartbeat(interpreter->heartbeat);
		check_for_rerender(interpreter, "window_message");
	}
}

static JSValue
js_window_postMessage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct el_window *elwin = (struct el_window *)(JS_GetOpaque(this_val, js_window_class_id));

	if (argc < 2) {
		return JS_UNDEFINED;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *data = stracpy(str);
	JS_FreeCString(ctx, str);

	str = JS_ToCStringLen(ctx, &len, argv[1]);
	if (!str) {
		mem_free_if(data);
		return JS_EXCEPTION;
	}
	char *targetOrigin = stracpy(str);
	JS_FreeCString(ctx, str);
	char *source = stracpy("TODO");

	JSValue val = get_messageEvent(ctx, data, targetOrigin, source);

	mem_free_if(data);
	mem_free_if(targetOrigin);
	mem_free_if(source);

	if (JS_IsNull(val) || !elwin) {
		return JS_UNDEFINED;
	}
	struct el_message *mess = (struct el_message *)mem_calloc(1, sizeof(*mess));
	if (!mess) {
		return JS_EXCEPTION;
	}
	mess->messageObject = JS_DupValue(ctx, val);
	mess->elwin = elwin;
	register_bottom_half(onmessage_run, mess);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_window_proto_funcs[] = {
	JS_CGETSET_DEF("closed", js_window_get_property_closed, NULL),
	JS_CGETSET_DEF("event", js_window_get_property_event, NULL),
	JS_CGETSET_DEF("innerHeight", js_window_get_property_innerHeight, NULL),
	JS_CGETSET_DEF("innerWidth", js_window_get_property_innerWidth, NULL),
	JS_CGETSET_DEF("location", js_window_get_property_location, js_window_set_property_location),
	JS_CGETSET_DEF("parent", js_window_get_property_parent, NULL),
	JS_CGETSET_DEF("self", js_window_get_property_self, NULL),
	JS_CGETSET_DEF("status", js_window_get_property_status, js_window_set_property_status),
	JS_CGETSET_DEF("top", js_window_get_property_top, NULL),
	JS_CGETSET_DEF("window", js_window_get_property_self, NULL),
	JS_CFUNC_DEF("addEventListener", 3, js_window_addEventListener),
	JS_CFUNC_DEF("alert", 1, js_window_alert),
	JS_CFUNC_DEF("clearInterval", 1, js_window_clearInterval),
	JS_CFUNC_DEF("clearTimeout", 1, js_window_clearTimeout),
	JS_CFUNC_DEF("getComputedStyle", 2, js_window_getComputedStyle),
	JS_CFUNC_DEF("open", 3, js_window_open),
	JS_CFUNC_DEF("postMessage", 3, js_window_postMessage),
	JS_CFUNC_DEF("removeEventListener", 3, js_window_removeEventListener),
	JS_CFUNC_DEF("setInterval", 2, js_window_setInterval),
	JS_CFUNC_DEF("setTimeout", 2, js_window_setTimeout),
	JS_CFUNC_DEF("toString", 0, js_window_toString)
};

static JSClassDef js_window_class = {
	"window",
	.finalizer = js_window_finalize,
	.gc_mark = js_window_mark,
};

int
js_window_init(JSContext *ctx)
{
	JSValue window_proto;

	/* create the window class */
	JS_NewClassID(&js_window_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_window_class_id, &js_window_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	window_proto = JS_NewObject(ctx);
	REF_JS(window_proto);
	JS_SetPropertyFunctionList(ctx, window_proto, js_window_proto_funcs, countof(js_window_proto_funcs));

	JS_SetClassProto(ctx, js_window_class_id, window_proto);
	JS_SetPropertyStr(ctx, global_obj, "window", JS_DupValue(ctx, window_proto));

	JS_SetPropertyFunctionList(ctx, global_obj, js_window_proto_funcs, countof(js_window_proto_funcs));

	JS_FreeValue(ctx, global_obj);

	return 0;
}

JSValue
getWindow(JSContext *ctx, struct view_state *vs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct el_window *elwin = (struct el_window *)mem_calloc(1, sizeof(*elwin));

	if (!elwin) {
		return JS_NULL;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

	JSValue win_obj = JS_NewObjectClass(ctx, js_window_class_id);
	JS_SetPropertyFunctionList(ctx, win_obj, js_window_proto_funcs, countof(js_window_proto_funcs));
	init_list(elwin->listeners);
	elwin->interpreter = interpreter;
	elwin->thisval = win_obj;
	elwin->vs = vs;

	if (vs) {
		vs->win_obj = win_obj;
	}
	JS_SetOpaque(win_obj, elwin);

	return win_obj;
}