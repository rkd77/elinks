/* The SEE window object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "elinks.h"

#include <see/see.h>

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
#include "ecmascript/see/checktype.h"
#include "ecmascript/see/input.h"
#include "ecmascript/see/strings.h"
#include "ecmascript/see/window.h"
#include "intl/gettext/libintl.h"
#include "main/select.h"
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

static struct js_window_object *js_get_global_object(void *);
static struct js_window_object *js_try_resolve_frame(struct document_view *, unsigned char *);
static void window_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static void window_put(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *, int);
static int window_canput(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static int window_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static void js_window_alert(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_window_open(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_setTimeout(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);

void location_goto(struct document_view *, unsigned char *);

struct SEE_objectclass js_window_object_class = {
	"window",
	window_get,
	window_put,
	window_canput,
	window_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	SEE_no_enumerator,
	NULL,
	NULL,
	NULL
};

static struct js_window_object *
js_get_global_object(void *data)
{
	struct global_object *g = (struct global_object *)data;
	return g->win;
}

static struct js_window_object *
js_try_resolve_frame(struct document_view *doc_view, unsigned char *id)
{
	struct session *ses = doc_view->session;
	struct frame *target;

	assert(ses);
	target = ses_find_frame(ses, id);
	if (!target) return NULL;
	if (target->vs.ecmascript_fragile)
		ecmascript_reset_state(&target->vs);
	if (!target->vs.ecmascript) return NULL;
	return js_get_global_object(target->vs.ecmascript->backend_data);
}

static void
window_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct js_window_object *win = (struct js_window_object *)o;
	struct view_state *vs = win->vs;

	if (p == s_closed) {
		SEE_SET_BOOLEAN(res, 0);
	} else if (p == s_self || p == s_parent || p == s_top || p == s_status) {
		SEE_SET_OBJECT(res, o);
#if 0
	} else if (p == s_parent || p == s_top) {
		struct document_view *doc_view = vs->doc_view;
		struct document_view *top_view = doc_view->session->doc_view;
		struct js_window_object *newjsframe;

		assert(top_view && top_view->vs);
		if (top_view->vs->ecmascript_fragile)
			ecmascript_reset_state(top_view->vs);
		if (!top_view->vs->ecmascript) {
			SEE_SET_UNDEFINED(res);
			return;
		}
		newjsframe = js_get_global_object(
			top_view->vs->ecmascript->backend_data);

		/* Keep this unrolled this way. Will have to check document.domain
		 * JS property. */
		/* Note that this check is perhaps overparanoid. If top windows
		 * is alien but some other child window is not, we should still
		 * let the script walk thru. That'd mean moving the check to
		 * other individual properties in this switch. */
		if (compare_uri(vs->uri, top_view->vs->uri, URI_HOST)) {
			SEE_SET_OBJECT(res, (struct SEE_object *)newjsframe);
		}
#endif
	} else if (p == s_alert) {
		SEE_SET_OBJECT(res, win->alert);
	} else if (p == s_open) {
		SEE_SET_OBJECT(res, win->open);
	} else if (p == s_setTimeout) {
		SEE_SET_OBJECT(res, win->setTimeout);
	} else if (p == s_location) {
		SEE_OBJECT_GET(interp, interp->Global, s_location, res);
	} else if (p == s_navigator) {
		SEE_OBJECT_GET(interp, interp->Global, s_navigator, res);
	} else {
		unsigned char *frame = see_string_to_unsigned_char(p);
		struct document_view *doc_view = vs->doc_view;
		struct js_window_object *obj;

		if (frame && (obj = js_try_resolve_frame(doc_view, frame))) {
			SEE_SET_OBJECT(res, (struct SEE_object *)obj);
		} else {
			SEE_SET_UNDEFINED(res);
		}
		mem_free_if(frame);
	}
}

static void
window_put(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *val, int attr)
{
	if (p == s_location) {
		struct js_window_object *win = (struct js_window_object *)o;
		struct view_state *vs = win->vs;
		struct document_view *doc_view = vs->doc_view;
		unsigned char *str = see_value_to_unsigned_char(interp, val);

		if (str) {
			location_goto(doc_view, str);
			mem_free(str);
		}
	} else if (p == s_status) {
		struct global_object *g = (struct global_object *)interp;
		struct js_window_object *win = g->win;
		struct view_state *vs = win->vs;
		struct document_view *doc_view = vs->doc_view;
		struct session *ses = doc_view->session;
		unsigned char *stat = see_value_to_unsigned_char(interp, val);

		mem_free_set(&ses->status.window_status, stat);
		print_screen_status(ses);
	}
}

static int
window_canput(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	if (p == s_location || p == s_status)
		return 1;
	return 0;
}

static int
window_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	/* all unknown properties return UNDEFINED value */
	return 1;
}


static void
js_window_alert(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct js_window_object *win = g->win;
	struct view_state *vs = win->vs;
	unsigned char *string;

	/* Do not check thisobj->objectclass.  ELinks sets this
	 * function as a property of both the window object and the
	 * global object, so thisobj may validly refer to either.  */

	SEE_SET_BOOLEAN(res, 1);
	if (argc < 1)
		return;

	string = see_value_to_unsigned_char(interp, argv[0]);
	if (!string || !*string) {
		mem_free_if(string);
		return;
	}

	info_box(vs->doc_view->session->tab->term, MSGBOX_FREE_TEXT,
		N_("JavaScript Alert"), ALIGN_CENTER, string);


}

static void
js_window_open(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct js_window_object *win = g->win;
	struct view_state *vs = win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct session *ses = doc_view->session;
	unsigned char *frame = NULL;
	unsigned char *url, *url2;
	struct uri *uri;
	struct SEE_value url_value;
	static time_t ratelimit_start;
	static int ratelimit_count;

	/* Do not check thisobj->objectclass.  ELinks sets this
	 * function as a property of both the window object and the
	 * global object, so thisobj may validly refer to either.  */

	SEE_SET_OBJECT(res, (struct SEE_object *)win);
	if (get_opt_bool("ecmascript.block_window_opening")) {
#ifdef CONFIG_LEDS
		set_led_value(ses->status.popup_led, 'P');
#endif
		return;
	}

	if (argc < 1) return;

	/* Ratelimit window opening. Recursive window.open() is very nice.
	 * We permit at most 20 tabs in 2 seconds. The ratelimiter is very
	 * rough but shall suffice against the usual cases. */

	if (!ratelimit_start || time(NULL) - ratelimit_start > 2) {
		ratelimit_start = time(NULL);
		ratelimit_count = 0;
	} else {
		ratelimit_count++;
		if (ratelimit_count > 20)
			return;
	}

	SEE_ToString(interp, argv[0], &url_value);
	url = see_string_to_unsigned_char(url_value.u.string);
	if (!url) return;
	trim_chars(url, ' ', 0);
	if (argc > 1) {
		struct SEE_value target_value;

		SEE_ToString(interp, argv[1], &target_value);
		frame = see_string_to_unsigned_char(target_value.u.string);
		if (!frame) {
			mem_free(url);
			return;
		}
	}
	/* TODO: Support for window naming and perhaps some window features? */

	url2 = join_urls(doc_view->document->uri, url);
	mem_free(url);
	if (!url2) {
		mem_free_if(frame);
		return;
	}
	uri = get_uri(url2, 0);
	mem_free(url2);
	if (!uri) {
		mem_free_if(frame);
		return;
	}

	if (frame && *frame && c_strcasecmp(frame, "_blank")) {
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			deo->target = stracpy(frame);
			/* target will be freed in delayed_goto_uri_frame */
			register_bottom_half(delayed_goto_uri_frame, deo);
			goto end;
		}
	}

	if (!get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home")
	    && !get_cmd_opt_bool("anonymous")
	    && can_open_in_new(ses->tab->term)) {
		open_uri_in_new_window(ses, uri, NULL, ENV_ANY,
				       CACHE_MODE_NORMAL, TASK_NONE);
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half(delayed_open, deo);
		}
	}

end:
	done_uri(uri);
	mem_free_if(frame);
}

static void
js_setTimeout(struct SEE_interpreter *interp, struct SEE_object *self,
	      struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	      struct SEE_value *res)
{
	struct ecmascript_interpreter *ei;
	unsigned char *code;
	int timeout;

	/* Do not check thisobj->objectclass.  ELinks sets this
	 * function as a property of both the window object and the
	 * global object, so thisobj may validly refer to either.  */

	if (argc != 2) return;
	ei = ((struct global_object *)interp)->interpreter;
	code = see_value_to_unsigned_char(interp, argv[0]);
	timeout = SEE_ToInt32(interp, argv[1]);
	ecmascript_set_timeout(ei, code, timeout);
}

void
init_js_window_object(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = interpreter->backend_data;
	struct SEE_interpreter *interp = &g->interp;
	struct SEE_value v;

	g->win = SEE_NEW(interp, struct js_window_object);

	g->win->object.objectclass = &js_window_object_class;
	g->win->object.Prototype = NULL;
	g->win->vs = interpreter->vs;

	SEE_SET_OBJECT(&v, (struct SEE_object *)g->win);
	SEE_OBJECT_PUT(interp, interp->Global, s_window, &v, 0);

	g->win->alert = SEE_cfunction_make(interp, js_window_alert, s_alert, 1);
	g->win->open = SEE_cfunction_make(interp, js_window_open, s_open, 3);
	g->win->setTimeout = SEE_cfunction_make(interp, js_setTimeout, s_setTimeout, 2);

	SEE_OBJECT_GET(interp, (struct SEE_object *)g->win, s_top, &v);
	SEE_OBJECT_PUT(interp, interp->Global, s_top, &v, 0);

	SEE_OBJECT_GET(interp, (struct SEE_object *)g->win, s_self, &v);
	SEE_OBJECT_PUT(interp, interp->Global, s_self, &v, 0);

	SEE_OBJECT_GET(interp, (struct SEE_object *)g->win, s_alert, &v);
	SEE_OBJECT_PUT(interp, interp->Global, s_alert, &v, 0);
	SEE_OBJECT_GET(interp, (struct SEE_object *)g->win, s_open, &v);
	SEE_OBJECT_PUT(interp, interp->Global, s_open, &v, 0);
	SEE_OBJECT_GET(interp, (struct SEE_object *)g->win, s_setTimeout, &v);
	SEE_OBJECT_PUT(interp, interp->Global, s_setTimeout, &v, 0);
}

void
checktime(struct SEE_interpreter *interp)
{
	struct global_object *g = (struct global_object *)interp;

	if (time(NULL) - g->exec_start > g->max_exec_time) {
		struct terminal *term = g->win->vs->doc_view->session->tab->term;
		/* A killer script! Alert! */
		ecmascript_timeout_dialog(term, g->max_exec_time);
		SEE_error_throw_string(interp, interp->Error, s_timeout);
	}
}
