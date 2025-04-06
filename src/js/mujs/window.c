/* The MuJS window object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

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
#include "js/mujs.h"
#include "js/mujs/css.h"
#include "js/mujs/element.h"
#include "js/mujs/keyboard.h"
#include "js/mujs/location.h"
#include "js/mujs/message.h"
#include "js/mujs/window.h"
#include "js/timer.h"
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
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

struct listener {
	LIST_HEAD_EL(struct listener);
	char *typ;
	const char *fun;
};

struct el_window {
	struct ecmascript_interpreter *interpreter;
	const char *thisval;
	LIST_OF(struct listener) listeners;
	char *onmessage;
	struct view_state *vs;
};

struct el_message {
	const char *messageObject;
	struct el_window *elwin;
};

static
void mjs_window_finalizer(js_State *J, void *val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct el_window *elwin = (struct el_window *)val;

	if (elwin) {
		struct listener *l;

		foreach(l, elwin->listeners) {
			mem_free_set(&l->typ, NULL);
		}
		free_list(elwin->listeners);
		mem_free(elwin);
	}
}

static void
mjs_window_get_property_closed(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushboolean(J, 0);
}

static void
mjs_window_get_property_event(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_push_keyboardEvent(J, NULL, "");
}

static void
mjs_window_get_property_innerHeight(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	assert(interpreter);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	js_pushnumber(J, doc_view->box.height * ses->tab->term->cell_height);
}

static void
mjs_window_get_property_innerWidth(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	assert(interpreter);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	struct session *ses = doc_view->session;

	if (!ses) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	js_pushnumber(J, doc_view->box.width * ses->tab->term->cell_width);
}

static void
mjs_window_get_property_location(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct el_window *elwin = (struct el_window *)js_touserdata(J, 0, "window");

	if (elwin && elwin->vs) {
		mjs_push_location(J, elwin->vs);
	} else {
		mjs_push_location(J, NULL);
	}
}

static void
mjs_window_set_property_location(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs;
	struct document_view *doc_view;
	vs = interpreter->vs;
	const char *url;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return;
	}
	doc_view = vs->doc_view;;
	url = js_tostring(J, 1);

	if (!url) {
		js_pushnull(J);
		return;
	}
	location_goto_const(doc_view, url, 0);
	js_pushundefined(J);
}

static void
mjs_window_get_property_parent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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

	js_pushundefined(J);
}

static void
mjs_window_get_property_self(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_copy(J, 0);
}

static void
mjs_window_get_property_status(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
mjs_window_set_property_status(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	char *text = stracpy(str);

	mem_free_set(&vs->doc_view->session->status.window_status, text);
	print_screen_status(vs->doc_view->session);

	js_pushundefined(J);
}

static void
mjs_window_get_property_top(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct document_view *doc_view;
	struct document_view *top_view;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushundefined(J);
		return;
	}
	doc_view = vs->doc_view;
	top_view = doc_view->session->doc_view;

	assert(top_view && top_view->vs);
	if (top_view->vs->ecmascript_fragile)
		ecmascript_reset_state(top_view->vs);
	if (!top_view->vs->ecmascript) {
		js_pushundefined(J);
		return;
	}
	/* Keep this unrolled this way. Will have to check document.domain
	 * JS property. */
	/* Note that this check is perhaps overparanoid. If top windows
	 * is alien but some other child window is not, we should still
	 * let the script walk thru. That'd mean moving the check to
	 * other individual properties in this switch. */
	if (compare_uri(vs->uri, top_view->vs->uri, URI_HOST)) {
		js_pushglobal((js_State *)top_view->vs->ecmascript->backend_data);
		return;
	}
		/* else */
		/****X*X*X*** SECURITY VIOLATION! RED ALERT, SHIELDS UP! ***X*X*X****\
		|* (Pasky was apparently looking at the Links2 JS code   .  ___ ^.^ *|
		\* for too long.)                                        `.(,_,)\o/ */
	js_pushundefined(J);
}

static void
mjs_window_alert(js_State *J)
{
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	assert(interpreter);
	struct view_state *vs = interpreter->vs;

	const char *str = js_tostring(J, 1);

	if (str) {
		char *string = stracpy(str);

		info_box(vs->doc_view->session->tab->term, MSGBOX_FREE_TEXT,
			N_("JavaScript Alert"), ALIGN_CENTER, string);
	}

	js_pushundefined(J);
}

static void
mjs_window_clearInterval(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *text = js_tostring(J, 1);

	if (!text) {
		js_pushundefined(J);
		return;
	}

	uintptr_t number = (uintptr_t)atoll(text);
	struct ecmascript_timeout *t = (struct ecmascript_timeout *)(number);

	if (found_in_map_timer(t)) {
		t->timeout_next = -1;
	}
	js_pushundefined(J);
}

static void
mjs_window_clearTimeout(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *text = js_tostring(J, 1);

	if (!text) {
		js_pushundefined(J);
		return;
	}

	uintptr_t number = (uintptr_t)atoll(text);
	struct ecmascript_timeout *t = (struct ecmascript_timeout *)(number);

	if (found_in_map_timer(t)) {
		t->timeout_next = -1;
	}
	js_pushundefined(J);
}

static void
mjs_window_getComputedStyle(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate(J, 1));
	mjs_push_CSSStyleDeclaration(J, el);
}

static void
mjs_window_open(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	const char *frame = NULL;
	char *url, *url2;
	struct uri *uri;
	static time_t ratelimit_start;
	static int ratelimit_count;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	ses = doc_view->session;

	if (get_opt_bool("ecmascript.block_window_opening", ses)) {
#ifdef CONFIG_LEDS
		set_led_value(ses->status.popup_led, 'P');
#endif
		js_pushundefined(J);
		return;
	}

	/* Ratelimit window opening. Recursive window.open() is very nice.
	 * We permit at most 20 tabs in 2 seconds. The ratelimiter is very
	 * rough but shall suffice against the usual cases. */
	if (!ratelimit_start || time(NULL) - ratelimit_start > 2) {
		ratelimit_start = time(NULL);
		ratelimit_count = 0;
	} else {
		ratelimit_count++;
		if (ratelimit_count > 20) {
			js_pushundefined(J);
			return;
		}
	}

	const char *str;
	str = js_tostring(J, 1);

	if (!str) {
		js_pushundefined(J);
		return;
	}

	url = stracpy(str);

	if (!url) {
		js_pushundefined(J);
		return;
	}
	trim_chars(url, ' ', 0);
	url2 = join_urls(doc_view->document->uri, url);
	mem_free(url);

	if (!url2) {
		js_pushundefined(J);
		return;
	}

	if (true) {
		frame = js_tostring(J, 2);

		if (!frame) {
			mem_free(url2);
			js_pushundefined(J);
			return;
		}
	}

	/* TODO: Support for window naming and perhaps some window features? */

	uri = get_uri(url2, URI_NONE);
	mem_free(url2);
	if (!uri) {
		js_pushundefined(J);
		return;
	}

	int ret = 0;

	if (frame && *frame && c_strcasecmp(frame, "_blank")) {
		struct delayed_open *deo = (struct delayed_open *)mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			deo->target = stracpy(frame);
			register_bottom_half(delayed_goto_uri_frame, deo);
			ret = 1;
			goto end;
		}
	}

	if (!get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home")
	    && !get_cmd_opt_bool("anonymous")
	    && can_open_in_new(ses->tab->term)) {
		open_uri_in_new_window(ses, uri, NULL, ENV_ANY,
				       CACHE_MODE_NORMAL, TASK_NONE);
		ret = 1;
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = (struct delayed_open *)mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half(delayed_open, deo);
			ret = 1;
		} else {
			ret = -1;
		}
	}

end:
	done_uri(uri);

	if (ret == -1) {
		js_pushundefined(J);
		return;
	}
	js_pushboolean(J, ret);
}

static void
mjs_window_scrollByLines(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct session *ses = doc_view->session;
	int steps = js_toint32(J, 1);

	vertical_scroll(ses, doc_view, steps);

	js_pushundefined(J);
}

static void
mjs_window_setInterval(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int timeout = js_toint32(J, 2);

	if (timeout <= 0) {
		js_pushundefined(J);
		return;
	}

	if (js_isstring(J, 1)) {
		const char *code = js_tostring(J, 1);

		if (!code) {
			js_pushundefined(J);
			return;
		}
		char *code2 = stracpy(code);

		if (code2) {
			struct ecmascript_timeout *id = ecmascript_set_timeout(J, code2, timeout, timeout);
			char res[32];
			snprintf(res, 31, "%" PRIuPTR, (uintptr_t)id);
			js_pushstring(J, res);
			return;
		}
	} else {
		js_copy(J, 1);
		const char *handle = js_ref(J);
		struct ecmascript_timeout *id = ecmascript_set_timeout2m(J, handle, timeout, timeout);
		char res[32];
		snprintf(res, 31, "%" PRIuPTR, (uintptr_t)id);
		js_pushstring(J, res);
		return;
	}
	js_pushundefined(J);
	return;
}


static void
mjs_window_setTimeout(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int timeout = js_toint32(J, 2);

	if (timeout < 0) {
		js_pushundefined(J);
		return;
	}

	if (js_isstring(J, 1)) {
		const char *code = js_tostring(J, 1);

		if (!code) {
			js_pushundefined(J);
			return;
		}
		char *code2 = stracpy(code);

		if (code2) {
			struct ecmascript_timeout *id = ecmascript_set_timeout(J, code2, timeout, 0);
			char res[32];
			snprintf(res, 31, "%" PRIuPTR, (uintptr_t)id);
			js_pushstring(J, res);
			return;
		}
	} else {
		js_copy(J, 1);
		const char *handle = js_ref(J);
		struct ecmascript_timeout *id = ecmascript_set_timeout2m(J, handle, timeout, 0);
		char res[32];
		snprintf(res, 31, "%" PRIuPTR, (uintptr_t)id);
		js_pushstring(J, res);
		return;
	}
	js_pushundefined(J);
	return;
}

static void
mjs_window_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[window object]");
}

static void
mjs_window_addEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct el_window *elwin = (struct el_window *)js_touserdata(J, 0, "window");

	if (!elwin) {
		elwin = (struct el_window *)mem_calloc(1, sizeof(*elwin));

		if (!elwin) {
			js_error(J, "out of memory");
			return;
		}
		init_list(elwin->listeners);
		elwin->interpreter = interpreter;
		elwin->thisval = js_ref(J);
		js_newuserdata(J, "window", elwin, mjs_window_finalizer);
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_error(J, "out of memory");
		return;
	}
	js_copy(J, 2);
	const char *fun = js_ref(J);

	struct listener *l;

	foreach(l, elwin->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (!strcmp(l->fun, fun)) {
			mem_free(method);
			js_pushundefined(J);
			return;
		}
	}
	struct listener *n = (struct listener *)mem_calloc(1, sizeof(*n));

	if (n) {
		n->typ = method;
		n->fun = fun;
		add_to_list_end(elwin->listeners, n);
	}
	js_pushundefined(J);
}

static void
mjs_window_removeEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct el_window *elwin = (struct el_window *)js_touserdata(J, 0, "window");

	if (!elwin) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_error(J, "out of memory");
		return;
	}
	js_copy(J, 2);
	const char *fun = js_ref(J);

	struct listener *l;

	foreach(l, elwin->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}

		if (!strcmp(l->fun, fun)) {
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			if (l->fun) js_unref(J, l->fun);
			mem_free(l);
			mem_free(method);
			js_pushundefined(J);
			return;
		}
	}
	mem_free(method);
	js_pushundefined(J);
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
		js_State *J = (js_State *)interpreter->backend_data;

		struct listener *l;

// TODO parameers for js_pcall
		foreach(l, elwin->listeners) {
			if (strcmp(l->typ, "message")) {
				continue;
			}
			js_getregistry(J, l->fun); /* retrieve the js function from the registry */
			js_getregistry(J, elwin->thisval);
			js_pcall(J, 0);
			js_pop(J, 1);
		}

		if (elwin->onmessage) {
			js_getregistry(J, elwin->onmessage); /* retrieve the js function from the registry */
			js_getregistry(J, mess->messageObject);
			js_pcall(J, 0);
			js_pop(J, 1);
		}
		check_for_rerender(interpreter, "window_message");
	}
}

static void
mjs_window_postMessage(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct el_window *elwin = (struct el_window *)js_touserdata(J, 0, "window");

	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	char *data = stracpy(str);

	const char *str2 = js_tostring(J, 2);

	if (!str2) {
		mem_free_if(data);
		js_error(J, "!str2");
		return;
	}
	char *targetOrigin = stracpy(str2);
	char *source = stracpy("TODO");

	mjs_push_messageEvent(J, data, targetOrigin, source);

	mem_free_if(data);
	mem_free_if(targetOrigin);
	mem_free_if(source);

	js_pop(J, 1);
	const char *val = js_ref(J);

	struct el_message *mess = (struct el_message *)mem_calloc(1, sizeof(*mess));
	if (!mess) {
		js_error(J, "out of memory");
		return;
	}
	mess->messageObject = val;
	mess->elwin = elwin;
	register_bottom_half(onmessage_run, mess);

	js_pushundefined(J);
}

int
mjs_window_init(js_State *J)
{
	js_pushglobal(J);
	{
		addmethod(J, "window.addEventListener", mjs_window_addEventListener, 3);
		addmethod(J, "window.alert", mjs_window_alert, 1);
		addmethod(J, "window.clearInterval", mjs_window_clearInterval, 1);
		addmethod(J, "window.clearTimeout", mjs_window_clearTimeout, 1);
		addmethod(J, "window.getComputedStyle", mjs_window_getComputedStyle, 2);
		addmethod(J, "window.open", mjs_window_open, 3);
		addmethod(J, "window.postMessage", mjs_window_postMessage, 3);
		addmethod(J, "window.removeEventListener", mjs_window_removeEventListener, 3);
		addmethod(J, "window.scrollByLines", mjs_window_scrollByLines, 1);
		addmethod(J, "window.setInterval", mjs_window_setInterval, 2);
		addmethod(J, "window.setTimeout", mjs_window_setTimeout, 2);
		addmethod(J, "window.toString", mjs_window_toString, 0);

		addproperty(J, "window.closed", mjs_window_get_property_closed, NULL);
		addproperty(J, "window.event", mjs_window_get_property_event, NULL);
		addproperty(J, "window.innerHeight", mjs_window_get_property_innerHeight, NULL);
		addproperty(J, "window.innerWidth", mjs_window_get_property_innerWidth, NULL);
		addproperty(J, "window.location", mjs_window_get_property_location, mjs_window_set_property_location);
		addproperty(J, "window.parent", mjs_window_get_property_parent, NULL);
		addproperty(J, "window.self", mjs_window_get_property_self, NULL);
		addproperty(J, "window.status", mjs_window_get_property_status, mjs_window_set_property_status);
		addproperty(J, "window.top", mjs_window_get_property_top, NULL);
	}
	js_defglobal(J, "window", 0);

	js_dostring(J, "Date.prototype.toGMTString = Date.prototype.toUTCString;");
	js_dostring(J, "String.prototype.endsWith || (String.prototype.endsWith = function(suffix) { return this.indexOf(suffix, this.length - suffix.length) !== -1; });");
	js_dostring(J, "String.prototype.startsWith || (String.prototype.startsWith = function(word) { return this.lastIndexOf(word, 0) === 0; });");


	return 0;
}

void
mjs_push_window(js_State *J, struct view_state *vs)
{
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct el_window *elwin = (struct el_window *)mem_calloc(1, sizeof(*elwin));

	if (!elwin) {
		js_error(J, "out of memory");
		return;
	}
	init_list(elwin->listeners);
	elwin->interpreter = interpreter;
	elwin->vs = vs;

	js_newobject(J);
	{
		js_newuserdata(J, "window", elwin, mjs_window_finalizer);
		addmethod(J, "window.addEventListener", mjs_window_addEventListener, 3);
		addmethod(J, "window.alert", mjs_window_alert, 1);
		addmethod(J, "window.clearInterval", mjs_window_clearInterval, 1);
		addmethod(J, "window.clearTimeout", mjs_window_clearTimeout, 1);
		addmethod(J, "window.getComputedStyle", mjs_window_getComputedStyle, 2);
		addmethod(J, "window.open", mjs_window_open, 3);
		addmethod(J, "window.postMessage", mjs_window_postMessage, 3);
		addmethod(J, "window.removeEventListener", mjs_window_removeEventListener, 3);
		addmethod(J, "window.scrollByLines", mjs_window_scrollByLines, 1);
		addmethod(J, "window.setInterval", mjs_window_setInterval, 2);
		addmethod(J, "window.setTimeout", mjs_window_setTimeout, 2);
		addmethod(J, "window.toString", mjs_window_toString, 0);

		addproperty(J, "closed", mjs_window_get_property_closed, NULL);
		addproperty(J, "event", mjs_window_get_property_event, NULL);
		addproperty(J, "innerHeight", mjs_window_get_property_innerHeight, NULL);
		addproperty(J, "innerWidth", mjs_window_get_property_innerWidth, NULL);
		addproperty(J, "location", mjs_window_get_property_location, mjs_window_set_property_location);
		addproperty(J, "parent", mjs_window_get_property_parent, NULL);
		addproperty(J, "self", mjs_window_get_property_self, NULL);
		addproperty(J, "status", mjs_window_get_property_status, mjs_window_set_property_status);
		addproperty(J, "top", mjs_window_get_property_top, NULL);
	}
	js_copy(J, 0);
	elwin->thisval = js_ref(J);

}
