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


static JSBool document_get_property(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp);

/* Each @document_class object must have a @window_class parent.  */
JSClass document_class = {
	"document",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	document_get_property, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

#ifdef CONFIG_COOKIES
static JSBool
document_get_property_cookie(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct string *cookies;
	JSClass* classPtr = JS_GetClass(obj);

	if (classPtr != &document_class)
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	cookies = send_cookies_js(vs->uri);

	if (cookies) {
		static unsigned char cookiestr[1024];

		strncpy(cookiestr, cookies->source, 1023);
		done_string(cookies);
		string_to_jsval(ctx, vp, cookiestr);
	} else {
		string_to_jsval(ctx, vp, "");
	}

	return JS_TRUE;
}

static JSBool
document_set_property_cookie(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, &document_class, NULL))
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	set_cookie(vs->uri, jsval_to_string(ctx, vp));

	return JS_TRUE;
}

#endif

static JSBool
document_get_property_location(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	JSClass* classPtr = JS_GetClass(obj);

	if (classPtr != &document_class)
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	JS_GetProperty(ctx, parent_win, "location", vp);

	return JS_TRUE;
}

static JSBool
document_set_property_location(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, &document_class, NULL))
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	location_goto(doc_view, jsval_to_string(ctx, vp));

	return JS_TRUE;
}


static JSBool
document_get_property_referrer(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	JSClass* classPtr = JS_GetClass(obj);

	if (classPtr != &document_class)
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;

	switch (get_opt_int("protocol.http.referer.policy", NULL)) {
	case REFERER_NONE:
		/* oh well */
		undef_to_jsval(ctx, vp);
		break;

	case REFERER_FAKE:
		string_to_jsval(ctx, vp, get_opt_str("protocol.http.referer.fake", NULL));
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

	return JS_TRUE;
}


static JSBool
document_get_property_title(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	JSClass* classPtr = JS_GetClass(obj);

	if (classPtr != &document_class)
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	string_to_jsval(ctx, vp, document->title);

	return JS_TRUE;
}

static JSBool
document_set_property_title(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, &document_class, NULL))
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	mem_free_set(&document->title, stracpy(jsval_to_string(ctx, vp)));
	print_screen_status(doc_view->session);

	return JS_TRUE;
}

static JSBool
document_get_property_url(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	JSClass* classPtr = JS_GetClass(obj);

	if (classPtr != &document_class)
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	astring_to_jsval(ctx, vp, get_uri_string(document->uri, URI_ORIGINAL));

	return JS_TRUE;
}

static JSBool
document_set_property_url(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSBool strict, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, &document_class, NULL))
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	location_goto(doc_view, jsval_to_string(ctx, vp));

	return JS_TRUE;
}


/* "cookie" is special; it isn't a regular property but we channel it to the
 * cookie-module. XXX: Would it work if "cookie" was defined in this array? */
JSPropertySpec document_props[] = {
#ifdef CONFIG_COOKIES
	{ "cookie",	0,	JSPROP_ENUMERATE | JSPROP_SHARED, JSOP_WRAPPER(document_get_property_cookie), JSOP_WRAPPER(document_set_property_cookie) },
#endif
	{ "location",	0,	JSPROP_ENUMERATE | JSPROP_SHARED, JSOP_WRAPPER(document_get_property_location), JSOP_WRAPPER(document_set_property_location) },
	{ "referrer",	0,	JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED, JSOP_WRAPPER(document_get_property_referrer), JSOP_NULLWRAPPER },
	{ "title",	0,	JSPROP_ENUMERATE | JSPROP_SHARED, JSOP_WRAPPER(document_get_property_title), JSOP_WRAPPER(document_set_property_title) }, /* TODO: Charset? */
	{ "url",	0,	JSPROP_ENUMERATE | JSPROP_SHARED, JSOP_WRAPPER(document_get_property_url), JSOP_WRAPPER(document_set_property_url) },
	{ NULL }
};


/* @document_class.getProperty */
static JSBool
document_get_property(JSContext *ctx, JSHandleObject hobj, JSHandleId hid, JSMutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	jsid id = *(hid._);

	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form *form;
	unsigned char *string;

	JSClass* classPtr = JS_GetClass(obj);

	if (classPtr != &document_class)
		return JS_FALSE;

	parent_win = JS_GetParent(obj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	string = jsid_to_string(ctx, &id);

	foreach (form, document->forms) {
		if (!form->name || c_strcasecmp(string, form->name))
			continue;

		object_to_jsval(ctx, vp, get_form_object(ctx, obj, find_form_view(doc_view, form)));
		break;
	}

	return JS_TRUE;
}

static JSBool document_write(JSContext *ctx, unsigned int argc, jsval *rval);
static JSBool document_writeln(JSContext *ctx, unsigned int argc, jsval *rval);

const spidermonkeyFunctionSpec document_funcs[] = {
	{ "write",		document_write,		1 },
	{ "writeln",		document_writeln,	1 },
	{ NULL }
};

static JSBool
document_write_do(JSContext *ctx, unsigned int argc, jsval *rval, int newline)
{
	jsval val;
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct string *ret = interpreter->ret;
	jsval *argv = JS_ARGV(ctx, rval);

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

	boolean_to_jsval(ctx, &val, 0);
	JS_SET_RVAL(ctx, rval, val);

	return JS_TRUE;
}

/* @document_funcs{"write"} */
static JSBool
document_write(JSContext *ctx, unsigned int argc, jsval *rval)
{

	return document_write_do(ctx, argc, rval, 0);
}

/* @document_funcs{"writeln"} */
static JSBool
document_writeln(JSContext *ctx, unsigned int argc, jsval *rval)
{
	return document_write_do(ctx, argc, rval, 1);
}
