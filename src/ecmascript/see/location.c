/* The SEE location and history objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "ecmascript/see/location.h"
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

static void delayed_goto(void *);
static void history_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static int history_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static void js_history_back(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_history_forward(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_history_go(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_location_toString(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void location_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static void location_put(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *, int);
static int location_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static int location_canput(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);

void location_goto(struct document_view *, unsigned char *);

struct js_history_object {
	struct SEE_object object;
	struct SEE_object *back;
	struct SEE_object *forward;
	struct SEE_object *go;
};

struct js_location_object {
	struct SEE_object object;
	struct SEE_object *toString;
};

struct delayed_goto {
	/* It might look more convenient to pass doc_view around but it could
	 * disappear during wild dances inside of frames or so. */
	struct view_state *vs;
	struct uri *uri;
};

struct SEE_objectclass js_history_object_class = {
	"history",
	history_get,
	SEE_no_put,
	SEE_no_canput,
	history_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	SEE_no_enumerator,
	NULL,
	NULL,
	NULL
};

struct SEE_objectclass js_location_object_class = {
	"location",
	location_get,
	location_put,
	location_canput,
	location_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	SEE_no_enumerator,
	NULL,
	NULL,
	NULL
};

static void
delayed_goto(void *data)
{
	struct delayed_goto *deg = data;

	assert(deg);
	if (deg->vs->doc_view
	    && deg->vs->doc_view == deg->vs->doc_view->session->doc_view) {
		goto_uri_frame(deg->vs->doc_view->session, deg->uri,
		               deg->vs->doc_view->name,
			       CACHE_MODE_NORMAL);
	}
	done_uri(deg->uri);
	mem_free(deg);
}

void
location_goto(struct document_view *doc_view, unsigned char *url)
{
	unsigned char *new_abs_url;
	struct uri *new_uri;
	struct delayed_goto *deg;

	/* Workaround for bug 611. Does not crash, but may lead to infinite loop.*/
	if (!doc_view) return;
	new_abs_url = join_urls(doc_view->document->uri,
	                        trim_chars(url, ' ', 0));
	if (!new_abs_url)
		return;
	new_uri = get_uri(new_abs_url, 0);
	mem_free(new_abs_url);
	if (!new_uri)
		return;
	deg = mem_calloc(1, sizeof(*deg));
	if (!deg) {
		done_uri(new_uri);
		return;
	}
	assert(doc_view->vs);
	deg->vs = doc_view->vs;
	deg->uri = new_uri;
	/* It does not seem to be very safe inside of frames to
	 * call goto_uri() right away. */
	register_bottom_half(delayed_goto, deg);
}


static void
history_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct js_history_object *history = (struct js_history_object *)o;

	if (p == s_back) {
		SEE_SET_OBJECT(res, history->back);
	} else if (p == s_forward) {
		SEE_SET_OBJECT(res, history->forward);
	} else if (p == s_go) {
		SEE_SET_OBJECT(res, history->go);
	} else {
		SEE_SET_UNDEFINED(res);
	}
}

static int
history_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	if (p == s_back || p == s_forward || p == s_go)
		return 1;
	return 0;
}

static void
js_history_back(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct session *ses = doc_view->session;

	see_check_class(interp, thisobj, &js_history_object_class);

	SEE_SET_NULL(res);
	go_back(ses);
}

static void
js_history_forward(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct session *ses = doc_view->session;

	see_check_class(interp, thisobj, &js_history_object_class);

	SEE_SET_NULL(res);
	go_unback(ses);
}

static void
js_history_go(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct session *ses = doc_view->session;
	unsigned char *str;
	int index;
	struct location *loc;

	see_check_class(interp, thisobj, &js_history_object_class);

	SEE_SET_NULL(res);
	if (argc < 1)
		return;

	str = see_value_to_unsigned_char(interp, argv[0]);
	if (!str)
		return;

	index  = atol(str);
	mem_free(str);

	for (loc = cur_loc(ses);
	     loc != (struct location *) &ses->history.history;
	     loc = index > 0 ? loc->next : loc->prev) {
		if (!index) {
			go_history(ses, loc);
			break;
		}

		index += index > 0 ? -1 : 1;
	}
}

static void
js_location_toString(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	unsigned char *string = get_uri_string(vs->uri, URI_ORIGINAL);
	struct SEE_string *str = string_to_SEE_string(interp, string);

	see_check_class(interp, thisobj, &js_location_object_class);

	mem_free_if(string);

	SEE_SET_STRING(res, str);
}

static void
location_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct js_location_object *loc = (struct js_location_object *)o;

	if (p == s_toString || p == s_toLocaleString) {
		SEE_SET_OBJECT(res, loc->toString);
	} else if (p == s_href) {
		struct global_object *g = (struct global_object *)interp;
		struct view_state *vs = g->win->vs;
		unsigned char *string = get_uri_string(vs->uri, URI_ORIGINAL);
		struct SEE_string *str = string_to_SEE_string(interp, string);

		mem_free_if(string);
		SEE_SET_STRING(res, str);
	} else {
		SEE_SET_UNDEFINED(res);
	}
}

static void
location_put(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *val, int attr)
{
	if (p == s_href) {
		struct global_object *g = (struct global_object *)interp;
		struct view_state *vs = g->win->vs;
		struct document_view *doc_view = vs->doc_view;
		unsigned char *url = see_value_to_unsigned_char(interp, val);

		location_goto(doc_view, url);
		mem_free(url);
	}
}

static int
location_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	if (p == s_toString || p == s_toLocaleString || p == s_href)
		return 1;
	return 0;
}

static int
location_canput(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	if (p == s_href)
		return 1;
	return 0;
}

void
init_js_history_object(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = interpreter->backend_data;
	struct SEE_interpreter *interp = &g->interp;
	struct SEE_value v;
	struct js_history_object *history = SEE_NEW(interp,
	 struct js_history_object);

	history->object.objectclass = &js_history_object_class;
	history->object.Prototype = NULL;

	SEE_SET_OBJECT(&v, (struct SEE_object *)history);
	SEE_OBJECT_PUT(interp, interp->Global, s_history, &v, 0);

	history->back = SEE_cfunction_make(interp, js_history_back, s_back, 0);
	history->forward = SEE_cfunction_make(interp, js_history_forward, s_forward, 0);
	history->go = SEE_cfunction_make(interp, js_history_go, s_go, 1);
}

void
init_js_location_object(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = interpreter->backend_data;
	struct SEE_interpreter *interp = &g->interp;
	struct SEE_value v;
	struct js_location_object *loc = SEE_NEW(interp,
		struct js_location_object);

	loc->object.objectclass = &js_location_object_class;
	loc->object.Prototype = NULL;

	SEE_SET_OBJECT(&v, (struct SEE_object *)loc);
	SEE_OBJECT_PUT(interp, interp->Global, s_location, &v, 0);

	loc->toString = SEE_cfunction_make(interp, js_location_toString, s_toString, 0);
}
