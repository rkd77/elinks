/* The SEE document object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

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
#include "ecmascript/see/document.h"
#include "ecmascript/see/form.h"
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

static void document_get(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *);
static void document_put(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *, struct SEE_value *, int);
static int document_canput(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static int document_hasproperty(struct SEE_interpreter *, struct SEE_object *, struct SEE_string *);
static void js_document_write(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);
static void js_document_writeln(struct SEE_interpreter *, struct SEE_object *, struct SEE_object *, int, struct SEE_value **, struct SEE_value *);

void location_goto(struct document_view *, unsigned char *);

struct SEE_objectclass js_document_object_class = {
	"document",
	document_get,
	document_put,
	document_canput,
	document_hasproperty,
	SEE_no_delete,
	SEE_no_defaultvalue,
	SEE_no_enumerator,
	NULL,
	NULL,
	NULL
};

static void
document_get(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *res)
{
	struct global_object *g = (struct global_object *)interp;
	struct js_window_object *win = g->win;
	struct view_state *vs = win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct js_document_object *doc = (struct js_document_object *)o;
	struct session *ses = doc_view->session;
	struct SEE_string *str;
	unsigned char *string;

	SEE_SET_UNDEFINED(res);

	if (p == s_cookie) {
#ifdef CONFIG_COOKIES
		struct string *cookies = send_cookies(vs->uri);

		if (cookies) {
			static unsigned char cookiestr[1024];
			strncpy(cookiestr, cookies->source, 1024);
			done_string(cookies);

			str = string_to_SEE_string(interp, cookiestr);
		} else {
			str = string_to_SEE_string(interp, "");
		}
		SEE_SET_STRING(res, str);
#endif
	} else if (p == s_title) {
		str = string_to_SEE_string(interp, document->title);
		SEE_SET_STRING(res, str);
	} else if (p == s_url) {
		string = get_uri_string(document->uri, URI_ORIGINAL);
		str = string_to_SEE_string(interp, string);
		mem_free_if(string);
		SEE_SET_STRING(res, str);
	} else if (p == s_location) {
		SEE_OBJECT_GET(interp, interp->Global, s_location, res);
	} else if (p == s_referrer) {
		switch (get_opt_int("protocol.http.referer.policy")) {
			case REFERER_NONE:
				SEE_SET_UNDEFINED(res);
				break;
			case REFERER_FAKE:
				str = string_to_SEE_string(interp,
				 get_opt_str("protocol.http.referer.fake"));
				SEE_SET_STRING(res, str);
				break;
			case REFERER_TRUE:
				if (ses->referrer) {
					string = get_uri_string(ses->referrer, URI_HTTP_REFERRER);
					str = string_to_SEE_string(interp, string);
					mem_free_if(string);
					SEE_SET_STRING(res, str);
				}
				break;
			case REFERER_SAME_URL:
				string = get_uri_string(document->uri, URI_HTTP_REFERRER);
				str = string_to_SEE_string(interp, string);
				mem_free_if(string);
				SEE_SET_STRING(res, str);
				break;

		}
	} else if (p == s_forms) {
		SEE_SET_OBJECT(res, doc->forms);
	} else if (p == s_write) {
		SEE_SET_OBJECT(res, doc->write);
	} else if (p == s_writeln) {
		SEE_SET_OBJECT(res, doc->writeln);
	} else {
		struct form *form;
		unsigned char *string = see_string_to_unsigned_char(p);
		struct form_view *form_view;
		struct js_form *form_object;

		if (!string) return;

		foreach (form, document->forms) {
			if (!form->name || c_strcasecmp(string, form->name))
				continue;
			form_view = find_form_view(doc_view, form);
			form_object = js_get_form_object(interp, doc, form_view);
			SEE_SET_OBJECT(res, (struct SEE_object *)form_object);
			break;
		}
		mem_free(string);
	}
}

static void
document_put(struct SEE_interpreter *interp, struct SEE_object *o,
	   struct SEE_string *p, struct SEE_value *val, int attr)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct js_document_object *doc = (struct js_document_object *)o;
	struct SEE_value res;
	unsigned char *string;

	if (p == s_forms) {
		SEE_ToObject(interp, val, &res);
		doc->forms = res.u.object;
	} else if (p == s_title) {
		string = see_value_to_unsigned_char(interp, val);
		mem_free_set(&document->title, string);
		print_screen_status(doc_view->session);
	} else if (p == s_location || p == s_url) {
		/* According to the specs this should be readonly but some
		 * broken sites still assign to it (i.e.
		 * http://www.e-handelsfonden.dk/validering.asp?URL=www.polyteknisk.dk).
		 * So emulate window.location. */
		string = see_value_to_unsigned_char(interp, val);
		location_goto(doc_view, string);
		mem_free_if(string);
	} else if (p == s_cookie) {
#ifdef CONFIG_COOKIES
		string = see_value_to_unsigned_char(interp, val);
		set_cookie(vs->uri, string);
		mem_free_if(string);
#endif
	}


}

static void
js_document_write_do(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res, int newline)
{
	struct global_object *g = (struct global_object *)interp;
	struct view_state *vs = g->win->vs;
	struct string *ret = g->ret;

	see_check_class(interp, thisobj, &js_document_object_class);

	if (argc >= 1 && ret) {
		int i = 0;

		for (; i < argc; ++i) {
			unsigned char *code;

			code = see_value_to_unsigned_char(interp, argv[i]);

			if (code) {
				add_to_string(ret, code);
				mem_free(code);
			}
		}

		if (newline)
			add_char_to_string(ret, '\n');
	}
#ifdef CONFIG_LEDS
	/* XXX: I don't know about you, but I have *ENOUGH* of those 'Undefined
	 * function' errors, I want to see just the useful ones. So just
	 * lighting a led and going away, no muss, no fuss. --pasky */
	/* TODO: Perhaps we can introduce ecmascript.error_report_unsupported
	 * -> "Show information about the document using some valid,
	 *  nevertheless unsupported methods/properties." --pasky too */

	set_led_value(vs->doc_view->session->status.ecmascript_led, 'J');
#endif
	SEE_SET_BOOLEAN(res, 0);
}

static void
js_document_write(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	js_document_write_do(interp, self, thisobj, argc, argv, res, 0);
}

static void
js_document_writeln(struct SEE_interpreter *interp, struct SEE_object *self,
	     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
	     struct SEE_value *res)
{
	js_document_write_do(interp, self, thisobj, argc, argv, res, 1);
}

static int
document_canput(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	if (p == s_location || p == s_url || p == s_cookie)
		return 1;
	return 0;
}

static int
document_hasproperty(struct SEE_interpreter *interp, struct SEE_object *o,
	      struct SEE_string *p)
{
	/* all unknown properties return UNDEFINED value */
	return 1;
}


void
init_js_document_object(struct ecmascript_interpreter *interpreter)
{
	struct global_object *g = interpreter->backend_data;
	struct SEE_interpreter *interp = &g->interp;
	struct SEE_value v;
	struct js_document_object *doc = SEE_NEW(interp,
		struct js_document_object);

	doc->object.objectclass = &js_document_object_class;
	doc->object.Prototype = NULL;

	SEE_SET_OBJECT(&v, (struct SEE_object *)doc);
	SEE_OBJECT_PUT(interp, interp->Global, s_document, &v, 0);

	doc->write = SEE_cfunction_make(interp, js_document_write, s_write, 1);
	doc->writeln = SEE_cfunction_make(interp, js_document_writeln, s_writeln, 1);
}
