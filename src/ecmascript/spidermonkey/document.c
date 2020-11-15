/* The SpiderMonkey document object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include <jsfriendapi.h>

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


static bool document_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

JSClassOps document_ops = {
	JS_PropertyStub, nullptr,
	document_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr
};


/* Each @document_class object must have a @window_class parent.  */
JSClass document_class = {
	"document",
	JSCLASS_HAS_PRIVATE,
	&document_ops
};

#ifdef CONFIG_COOKIES
static bool
document_get_property_cookie(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct string *cookies;

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return false;
	}
	cookies = send_cookies_js(vs->uri);

	if (cookies) {
		static unsigned char cookiestr[1024];

		strncpy(cookiestr, cookies->source, 1023);
		done_string(cookies);
		args.rval().setString(JS_NewStringCopyZ(ctx, cookiestr));
	} else {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
	}

	return true;
}

static bool
document_set_property_cookie(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct string *cookies;

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return false;
	}
	set_cookie(vs->uri, JS_EncodeString(ctx, args[0].toString()));

	return true;
}

#endif

static bool
document_get_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	JS_GetProperty(ctx, parent_win, "location", args.rval());

	return true;
}

static bool
document_set_property_location(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct document_view *doc_view;

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	location_goto(doc_view, JS_EncodeString(ctx, args[0].toString()));

	return true;
}


static bool
document_get_property_referrer(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;

	switch (get_opt_int("protocol.http.referer.policy", NULL)) {
	case REFERER_NONE:
		/* oh well */
		args.rval().setUndefined();
		break;

	case REFERER_FAKE:
		args.rval().setString(JS_NewStringCopyZ(ctx, get_opt_str("protocol.http.referer.fake", NULL)));
		break;

	case REFERER_TRUE:
		/* XXX: Encode as in add_url_to_httset_prop_string(&prop, ) ? --pasky */
		if (ses->referrer) {
			unsigned char *str = get_uri_string(ses->referrer, URI_HTTP_REFERRER);

			if (str) {
				args.rval().setString(JS_NewStringCopyZ(ctx, str));
				mem_free(str);
			} else {
				args.rval().setUndefined();
			}
		}
		break;

	case REFERER_SAME_URL:
		unsigned char *str = get_uri_string(document->uri, URI_HTTP_REFERRER);

		if (str) {
			args.rval().setString(JS_NewStringCopyZ(ctx, str));
			mem_free(str);
		} else {
			args.rval().setUndefined();
		}
		break;
	}

	return true;
}


static bool
document_get_property_title(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	args.rval().setString(JS_NewStringCopyZ(ctx, document->title));

	return true;
}

static bool
document_set_property_title(JSContext *ctx, int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
//	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	assert(JS_InstanceOf(ctx, hobj, &document_class, NULL));
	if_assert_failed return false;

//	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
//	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, hobj,
				   &document_class, NULL);
	if (!vs || !vs->doc_view) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	mem_free_set(&document->title, stracpy(JS_EncodeString(ctx, args[0].toString())));
	print_screen_status(doc_view->session);

	return true;
}

static bool
document_get_property_url(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	unsigned char *str = get_uri_string(document->uri, URI_ORIGINAL);

	if (str) {
		args.rval().setString(JS_NewStringCopyZ(ctx, str));
		mem_free(str);
	} else {
		args.rval().setUndefined();
	}

	return true;
}

static bool
document_set_property_url(JSContext *ctx, int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedObject parent_win(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	location_goto(doc_view, JS_EncodeString(ctx, args[0].toString()));

	return true;
}


/* "cookie" is special; it isn't a regular property but we channel it to the
 * cookie-module. XXX: Would it work if "cookie" was defined in this array? */
JSPropertySpec document_props[] = {
#ifdef CONFIG_COOKIES
	JS_PSGS("cookie", document_get_property_cookie, document_set_property_cookie, JSPROP_ENUMERATE),
#endif
	JS_PSGS("location",	document_get_property_location, document_set_property_location, JSPROP_ENUMERATE),
	JS_PSG("referrer",	document_get_property_referrer, JSPROP_ENUMERATE),
	JS_PSGS("title",	document_get_property_title, document_set_property_title, JSPROP_ENUMERATE), /* TODO: Charset? */
	JS_PSGS("url",	document_get_property_url, document_set_property_url, JSPROP_ENUMERATE),
	{ NULL }
};


/* @document_class.getProperty */
static bool
document_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	JS::RootedObject parent_win(ctx);	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form *form;
	unsigned char *string;

	JSClass* classPtr = JS_GetClass(hobj);

	if (classPtr != &document_class)
		return false;

	parent_win = js::GetGlobalForObjectCrossCompartment(hobj);
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;

	if (!JSID_IS_STRING(hid)) {
		return true;
	}
	string = jsid_to_string(ctx, hid);

	foreach (form, document->forms) {
		if (!form->name || c_strcasecmp(string, form->name))
			continue;

		hvp.setObject(*get_form_object(ctx, hobj, find_form_view(doc_view, form)));
		break;
	}

	return true;
}

static bool document_write(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool document_writeln(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec document_funcs[] = {
	{ "write",		document_write,		1 },
	{ "writeln",		document_writeln,	1 },
	{ NULL }
};

static bool
document_write_do(JSContext *ctx, unsigned int argc, JS::Value *rval, int newline)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
	JS::Value val;
//	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct string *ret = interpreter->ret;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

	if (argc >= 1 && ret) {
		int i = 0;

		for (; i < argc; ++i) {
			unsigned char *code = jsval_to_string(ctx, args[i]);

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

	args.rval().setBoolean(false);

	return true;
}

/* @document_funcs{"write"} */
static bool
document_write(JSContext *ctx, unsigned int argc, JS::Value *rval)
{

	return document_write_do(ctx, argc, rval, 0);
}

/* @document_funcs{"writeln"} */
static bool
document_writeln(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	return document_write_do(ctx, argc, rval, 1);
}
