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

void location_goto(struct document_view *, unsigned char *);

struct SEE_objectclass js_window_object_class = {
	NULL,
	window_get,
	window_put,
	window_canput,
	window_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	NULL,
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

	checktime(interp);
	if (p == s_closed) {
		SEE_SET_BOOLEAN(res, 0);
	} else if (p == s_self) {
		SEE_SET_OBJECT(res, o);
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

	} else if (p == s_alert) {
		SEE_SET_OBJECT(res, win->alert);
	} else if (p == s_open) {
		SEE_SET_OBJECT(res, win->open);
	} else if (p == s_location) {
		SEE_OBJECT_GET(interp, interp->Global, s_location, res);
	} else {
		unsigned char *frame = SEE_string_to_unsigned_char(p);
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
	checktime(interp);
	if (p == s_location) {
		struct js_window_object *win = (struct js_window_object *)o;
		struct view_state *vs = win->vs;
		struct document_view *doc_view = vs->doc_view;
		unsigned char *str = SEE_value_to_unsigned_char(interp, val);

		if (str) {
			location_goto(doc_view, str);
			mem_free(str);
		}
	}
}

static int
window_canput(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	checktime(interp);
	if (p == s_location)
		return 1;
	return 0;
}

static int
window_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	checktime(interp);
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

	checktime(interp);
	SEE_SET_BOOLEAN(res, 1);
	if (argc < 1)
		return;

	string = SEE_value_to_unsigned_char(interp, argv[0]);
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
	unsigned char *target = NULL;
	unsigned char *url, *url2;
	struct uri *uri;
	struct SEE_value url_value, target_value;
#if 0
	static time_t ratelimit_start;
	static int ratelimit_count;
#endif
	checktime(interp);
	SEE_SET_OBJECT(res, (struct SEE_object *)win);
	if (get_opt_bool("ecmascript.block_window_opening")) {
#ifdef CONFIG_LEDS
		set_led_value(ses->status.popup_led, 'P');
#endif
		return;
	}

	if (argc < 1) return;
#if 0
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
#endif
	SEE_ToString(interp, argv[0], &url_value);
	if (argc > 1) {
		/* Because of gradual rendering window.open is called many
		 * times with the same arguments.
		 * This workaround remembers NUMBER_OF_URLS_TO_REMEMBER last
		 * opened URLs and do not let open them again.
		 */
#define NUMBER_OF_URLS_TO_REMEMBER 8
		static struct {
			struct SEE_string *url;
			struct SEE_string *target;
		} strings[NUMBER_OF_URLS_TO_REMEMBER];
		static int indeks = 0;
		int i;

		SEE_ToString(interp, argv[1], &target_value);
		for (i = 0; i < NUMBER_OF_URLS_TO_REMEMBER; i++) {
			if (!(strings[i].url && strings[i].target))
				continue;
			if (!SEE_string_cmp(url_value.u.string, strings[i].url)
			    && !SEE_string_cmp(target_value.u.string, strings[i].target))
			 	return;
		}
		strings[indeks].url = url_value.u.string;
		strings[indeks].target = target_value.u.string;
		indeks++;
		if (indeks >= NUMBER_OF_URLS_TO_REMEMBER) indeks = 0;
#undef NUMBER_OF_URLS_TO_REMEMBER
	}

	url = SEE_string_to_unsigned_char(url_value.u.string);
	if (!url) return;

	/* TODO: Support for window naming and perhaps some window features? */

	url2 = join_urls(doc_view->document->uri,
	                trim_chars(url, ' ', 0));
	mem_free(url);
	if (!url2) return;
	uri = get_uri(url2, 0);
	mem_free(url2);
	if (!uri) return;

	if (argc > 1) {
		target = SEE_string_to_unsigned_char(target_value.u.string);
	}

	if (target && *target && strcasecmp(target, "_blank")) {
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			deo->target = target;
			/* target will be freed in delayed_goto_uri_frame */
			register_bottom_half(delayed_goto_uri_frame, deo);
			goto end;
		}
	}

	mem_free_if(target);
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

}


void
init_js_window_object(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = interpreter->backend_data;
	struct SEE_interpreter *interp = &g->interp;
	struct SEE_value v;

	g->win = SEE_NEW(interp, struct js_window_object);

	g->win->object.objectclass = &js_window_object_class;
	g->win->object.objectclass->Class = s_window;
	g->win->object.Prototype = NULL;
	g->win->vs = interpreter->vs;

	SEE_SET_OBJECT(&v, (struct SEE_object *)g->win);
	SEE_OBJECT_PUT(interp, interp->Global, s_window, &v, 0);

	g->win->alert = SEE_cfunction_make(interp, js_window_alert, s_alert, 1);
	g->win->open = SEE_cfunction_make(interp, js_window_open, s_open, 3);

	SEE_OBJECT_GET(interp, (struct SEE_object *)g->win, s_top, &v);
	SEE_OBJECT_PUT(interp, interp->Global, s_top, &v, 0);

	SEE_OBJECT_GET(interp, (struct SEE_object *)g->win, s_self, &v);
	SEE_OBJECT_PUT(interp, interp->Global, s_self, &v, 0);

	SEE_OBJECT_GET(interp, (struct SEE_object *)g->win, s_alert, &v);
	SEE_OBJECT_PUT(interp, interp->Global, s_alert, &v, 0);
	SEE_OBJECT_GET(interp, (struct SEE_object *)g->win, s_open, &v);
	SEE_OBJECT_PUT(interp, interp->Global, s_open, &v, 0);
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
