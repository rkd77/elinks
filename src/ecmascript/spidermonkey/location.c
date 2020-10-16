/* The SpiderMonkey location and history objects implementation. */

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
#include "ecmascript/spidermonkey/location.h"
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


static bool history_back(JSContext *ctx, unsigned int argc, jsval *rval);
static bool history_forward(JSContext *ctx, unsigned int argc, jsval *rval);
static bool history_go(JSContext *ctx, unsigned int argc, jsval *rval);

JSClass history_class = {
	"history",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
};

const spidermonkeyFunctionSpec history_funcs[] = {
	{ "back",		history_back,		0 },
	{ "forward",		history_forward,	0 },
	{ "go",			history_go,		1 },
	{ NULL }
};

/* @history_funcs{"back"} */
static bool
history_back(JSContext *ctx, unsigned int argc, jsval *rval)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

	go_back(ses);

/* history_back() must return 0 for onClick to cause displaying previous page
 * and return non zero for <a href="javascript:history.back()"> to prevent
 * "calculating" new link. Returned value 2 is changed to 0 in function
 * spidermonkey_eval_boolback */
	args.rval().setNull();
	return 2;
}

/* @history_funcs{"forward"} */
static bool
history_forward(JSContext *ctx, unsigned int argc, jsval *rval)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

	go_unback(ses);

	args.rval().setNull();
	return 2;
}

/* @history_funcs{"go"} */
static bool
history_go(JSContext *ctx, unsigned int argc, jsval *rval)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);

	struct location *loc;

	if (argc != 1)
		return true;

	int index  = args[0].toInt32();

	for (loc = cur_loc(ses);
	     loc != (struct location *) &ses->history.history;
	     loc = index > 0 ? loc->next : loc->prev) {
		if (!index) {
			go_history(ses, loc);
			break;
		}

		index += index > 0 ? -1 : 1;
	}

	args.rval().setNull();
	return 2;
}


static bool location_get_property_href(JSContext *ctx, unsigned int argc, jsval *vp);
static bool location_set_property_href(JSContext *ctx, unsigned int argc, jsval *vp);

/* Each @location_class object must have a @window_class parent.  */
JSClass location_class = {
	"location",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, nullptr,
	JS_PropertyStub, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in location[-1];
 * future versions of ELinks may change the numbers.  */
enum location_prop {
	JSP_LOC_HREF = -1,
};
JSPropertySpec location_props[] = {
	JS_PSGS("href",	location_get_property_href, location_set_property_href, JSPROP_ENUMERATE),
	{ NULL }
};


static bool
location_get_property_href(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &location_class, NULL))
		return false;

	JS::RootedObject parent_win(ctx, JS_GetParent(hobj));
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return false;
	}

	unsigned char *str = get_uri_string(vs->uri, URI_ORIGINAL);

	if (!str) {
		return false;
	}

	args.rval().setString(JS_NewStringCopyZ(ctx, str));
	mem_free(str);

	return true;
}

static bool
location_set_property_href(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &location_class, NULL))
		return false;

	JS::RootedObject parent_win(ctx, JS_GetParent(hobj));
	assert(JS_InstanceOf(ctx, parent_win, &window_class, NULL));
	if_assert_failed return false;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   &window_class, NULL);
	if (!vs) {
		return;
	}
	doc_view = vs->doc_view;
	location_goto(doc_view, JS_EncodeString(ctx, args[0].toString()));

	return true;
}


static bool location_toString(JSContext *ctx, unsigned int argc, jsval *rval);

const spidermonkeyFunctionSpec location_funcs[] = {
	{ "toString",		location_toString,	0 },
	{ "toLocaleString",	location_toString,	0 },
	{ NULL }
};

/* @location_funcs{"toString"}, @location_funcs{"toLocaleString"} */
static bool
location_toString(JSContext *ctx, unsigned int argc, jsval *rval)
{
	jsval val;
	JSObject *obj = JS_THIS_OBJECT(ctx, rval);
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, obj);
	JS::RootedValue r_val(ctx, val);
	bool ret = JS_GetProperty(ctx, hobj, "href", &r_val);

	args.rval().set(val);
	return ret;
}

struct delayed_goto {
	/* It might look more convenient to pass doc_view around but it could
	 * disappear during wild dances inside of frames or so. */
	struct view_state *vs;
	struct uri *uri;
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
