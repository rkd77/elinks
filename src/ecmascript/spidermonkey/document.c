/* The SpiderMonkey document object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"

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
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/window.h"
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


static JSBool document_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool document_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

/* Each @document_class object must have a @window_class parent.  */
const JSClass document_class = {
	"document",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	document_get_property, document_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in document[-1];
 * future versions of ELinks may change the numbers.  */
enum document_prop {
	JSP_DOC_LOC   = -1,
	JSP_DOC_REF   = -2,
	JSP_DOC_TITLE = -3,
	JSP_DOC_URL   = -4,
};
/* "cookie" is special; it isn't a regular property but we channel it to the
 * cookie-module. XXX: Would it work if "cookie" was defined in this array? */
const JSPropertySpec document_props[] = {
	{ "location",	JSP_DOC_LOC,	JSPROP_ENUMERATE },
	{ "referrer",	JSP_DOC_REF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "title",	JSP_DOC_TITLE,	JSPROP_ENUMERATE }, /* TODO: Charset? */
	{ "url",	JSP_DOC_URL,	JSPROP_ENUMERATE },
	{ NULL }
};

/* @document_class.getProperty */
static JSBool
document_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &document_class, NULL))
		return JS_FALSE;
	parent_win = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;

	if (JSVAL_IS_STRING(id)) {
		struct form *form;
		unsigned char *string = jsval_to_string(ctx, &id);

#ifdef CONFIG_COOKIES
		if (!strcmp(string, "cookie")) {
			struct string *cookies = send_cookies(vs->uri);

			if (cookies) {
				static unsigned char cookiestr[1024];

				strncpy(cookiestr, cookies->source, 1024);
				done_string(cookies);

				string_to_jsval(ctx, vp, cookiestr);
			} else {
				string_to_jsval(ctx, vp, "");
			}
			return JS_TRUE;
		}
#endif
		foreach (form, document->forms) {
			if (!form->name || c_strcasecmp(string, form->name))
				continue;

			object_to_jsval(ctx, vp, get_form_object(ctx, obj, find_form_view(doc_view, form)));
			break;
		}
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_LOC:
		JS_GetProperty(ctx, parent_win, "location", vp);
		break;
	case JSP_DOC_REF:
		switch (get_opt_int("protocol.http.referer.policy")) {
		case REFERER_NONE:
			/* oh well */
			undef_to_jsval(ctx, vp);
			break;

		case REFERER_FAKE:
			string_to_jsval(ctx, vp, get_opt_str("protocol.http.referer.fake"));
			break;

		case REFERER_TRUE:
			/* XXX: Encode as in add_url_to_httset_prop_string(&prop, ) ? --pasky */
			if (ses->referrer) {
				astring_to_jsval(ctx, vp, get_uri_string(ses->referrer, URI_HTTP_REFERRER));
			}
			break;

		case REFERER_SAME_URL:
			astring_to_jsval(ctx, vp, get_uri_string(document->uri, URI_HTTP_REFERRER));
			break;
		}
		break;
	case JSP_DOC_TITLE:
		string_to_jsval(ctx, vp, document->title);
		break;
	case JSP_DOC_URL:
		astring_to_jsval(ctx, vp, get_uri_string(document->uri, URI_ORIGINAL));
		break;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		break;
	}

	return JS_TRUE;
}

/* @document_class.setProperty */
static JSBool
document_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &document_class, NULL))
		return JS_FALSE;
	parent_win = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;

	if (JSVAL_IS_STRING(id)) {
#ifdef CONFIG_COOKIES
		if (!strcmp(jsval_to_string(ctx, &id), "cookie")) {
			set_cookie(vs->uri, jsval_to_string(ctx, vp));
			/* Do NOT touch our .cookie property, evil
			 * SpiderMonkey!! */
			return JS_FALSE;
		}
#endif
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_TITLE:
		mem_free_set(&document->title, stracpy(jsval_to_string(ctx, vp)));
		print_screen_status(doc_view->session);
		break;
	case JSP_DOC_LOC:
	case JSP_DOC_URL:
		/* According to the specs this should be readonly but some
		 * broken sites still assign to it (i.e.
		 * http://www.e-handelsfonden.dk/validering.asp?URL=www.polyteknisk.dk).
		 * So emulate window.location. */
		location_goto(doc_view, jsval_to_string(ctx, vp));
		break;
	}

	return JS_TRUE;
}

static JSBool document_write(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool document_writeln(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

const spidermonkeyFunctionSpec document_funcs[] = {
	{ "write",		document_write,		1 },
	{ "writeln",		document_writeln,	1 },
	{ NULL }
};

static JSBool
document_write_do(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval, int newline)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct string *ret = interpreter->ret;

	if (argc >= 1 && ret) {
		int i = 0;

		for (; i < argc; ++i) {
			unsigned char *code = jsval_to_string(ctx, &argv[i]);

			add_to_string(ret, code);
		}

		if (newline)
			add_char_to_string(ret, '\n');
	}
	/* XXX: I don't know about you, but I have *ENOUGH* of those 'Undefined
	 * function' errors, I want to see just the useful ones. So just
	 * lighting a led and going away, no muss, no fuss. --pasky */
	/* TODO: Perhaps we can introduce ecmascript.error_report_unsupported
	 * -> "Show information about the document using some valid,
	 *  nevertheless unsupported methods/properties." --pasky too */

#ifdef CONFIG_LEDS
	set_led_value(interpreter->vs->doc_view->session->status.ecmascript_led, 'J');
#endif

	boolean_to_jsval(ctx, rval, 0);

	return JS_TRUE;
}

/* @document_funcs{"write"} */
static JSBool
document_write(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{

	return document_write_do(ctx, obj, argc, argv, rval, 0);
}

/* @document_funcs{"writeln"} */
static JSBool
document_writeln(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return document_write_do(ctx, obj, argc, argv, rval, 1);
}
