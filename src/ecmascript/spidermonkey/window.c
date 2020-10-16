/* The SpiderMonkey window object implementation. */

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


static bool window_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool window_get_property_closed(JSContext *cx, unsigned int argc, jsval *vp);
static bool window_get_property_parent(JSContext *ctx, unsigned int argc, jsval *vp);
static bool window_get_property_self(JSContext *ctx, unsigned int argc, jsval *vp);
static bool window_get_property_status(JSContext *ctx, unsigned int argc, jsval *vp);
static bool window_set_property_status(JSContext *ctx, unsigned int argc, jsval *vp);
static bool window_get_property_top(JSContext *ctx, unsigned int argc, jsval *vp);

JSClass window_class = {
	"window",
	JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS,	/* struct view_state * */
	JS_PropertyStub, nullptr,
	window_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
};


/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in window[-1];
 * future versions of ELinks may change the numbers.  */
enum window_prop {
	JSP_WIN_CLOSED = -1,
	JSP_WIN_PARENT = -2,
	JSP_WIN_SELF   = -3,
	JSP_WIN_STATUS = -4,
	JSP_WIN_TOP    = -5,
};
/* "location" is special because we need to simulate "location.href"
 * when the code is asking directly for "location". We do not register
 * it as a "known" property since that was yielding strange bugs
 * (SpiderMonkey was still asking us about the "location" string after
 * assigning to it once), instead we do just a little string
 * comparing. */
JSPropertySpec window_props[] = {
	JS_PSG("closed",	window_get_property_closed, JSPROP_ENUMERATE),
	JS_PSG("parent",	window_get_property_parent, JSPROP_ENUMERATE),
	JS_PSG("self",	window_get_property_self, JSPROP_ENUMERATE),
	JS_PSGS("status",	window_get_property_status, window_set_property_status, 0),
	JS_PSG("top",	window_get_property_top, JSPROP_ENUMERATE),
	JS_PSG("window",	window_get_property_self, JSPROP_ENUMERATE),
	{ NULL }
};


static JSObject *
try_resolve_frame(struct document_view *doc_view, unsigned char *id)
{
	struct session *ses = doc_view->session;
	struct frame *target;

	assert(ses);
	target = ses_find_frame(ses, id);
	if (!target) return NULL;
	if (target->vs.ecmascript_fragile)
		ecmascript_reset_state(&target->vs);
	if (!target->vs.ecmascript) return NULL;
	return JS::CurrentGlobalOrNull(target->vs.ecmascript->backend_data);
}

#if 0
static struct frame_desc *
find_child_frame(struct document_view *doc_view, struct frame_desc *tframe)
{
	struct frameset_desc *frameset = doc_view->document->frame_desc;
	int i;

	if (!frameset)
		return NULL;

	for (i = 0; i < frameset->n; i++) {
		struct frame_desc *frame = &frameset->frame_desc[i];

		if (frame == tframe)
			return frame;
	}

	return NULL;
}
#endif

/* @window_class.getProperty */
static bool
window_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	jsid id = hid.get();

	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &window_class, NULL))
		return false;

	vs = JS_GetInstancePrivate(ctx, hobj, &window_class, NULL);

	/* No need for special window.location measurements - when
	 * location is then evaluated in string context, toString()
	 * is called which we overrode for that class below, so
	 * everything's fine. */
	if (JSID_IS_STRING(id)) {
		struct document_view *doc_view = vs->doc_view;
		JSObject *obj = try_resolve_frame(doc_view, jsid_to_string(ctx, &id));
		/* TODO: Try other lookups (mainly element lookup) until
		 * something yields data. */
		if (obj) {
			object_to_jsval(ctx, vp, obj);
		}
		return true;
	}

	if (!JSID_IS_INT(id))
		return true;

	undef_to_jsval(ctx, vp);

	switch (JSID_TO_INT(id)) {
	case JSP_WIN_CLOSED:
		/* TODO: It will be a major PITA to implement this properly.
		 * Well, perhaps not so much if we introduce reference tracking
		 * for (struct session)? Still... --pasky */
		boolean_to_jsval(ctx, vp, 0);
		break;
	case JSP_WIN_SELF:
		object_to_jsval(ctx, vp, obj);
		break;
	case JSP_WIN_PARENT:
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
#if 0
	{
		/* This is horrible. */
		struct document_view *doc_view = vs->doc_view;
		struct session *ses = doc_view->session;
		struct frame_desc *frame = doc_view->document->frame;

		if (!ses->doc_view->document->frame_desc) {
			INTERNAL("Looking for parent but there're no frames.");
			break;
		}
		assert(frame);
		doc_view = ses->doc_view;
		if (find_child_frame(doc_view, frame))
			goto found_parent;
		foreach (doc_view, ses->scrn_frames) {
			if (find_child_frame(doc_view, frame))
				goto found_parent;
		}
		INTERNAL("Cannot find frame %s parent.",doc_view->name);
		break;

found_parent:
		some_domain_security_check();
		if (doc_view->vs.ecmascript_fragile)
			ecmascript_reset_state(&doc_view->vs);
		assert(doc_view->ecmascript);
		object_to_jsval(ctx, vp, JS_GetGlobalForScopeChain(doc_view->ecmascript->backend_data));
		break;
	}
#endif
	case JSP_WIN_STATUS:
		return false;
	case JSP_WIN_TOP:
	{
		struct document_view *doc_view = vs->doc_view;
		struct document_view *top_view = doc_view->session->doc_view;
		JSObject *newjsframe;

		assert(top_view && top_view->vs);
		if (top_view->vs->ecmascript_fragile)
			ecmascript_reset_state(top_view->vs);
		if (!top_view->vs->ecmascript)
			break;
		newjsframe = JS::CurrentGlobalOrNull(top_view->vs->ecmascript->backend_data);

		/* Keep this unrolled this way. Will have to check document.domain
		 * JS property. */
		/* Note that this check is perhaps overparanoid. If top windows
		 * is alien but some other child window is not, we should still
		 * let the script walk thru. That'd mean moving the check to
		 * other individual properties in this switch. */
		if (compare_uri(vs->uri, top_view->vs->uri, URI_HOST))
			object_to_jsval(ctx, vp, newjsframe);
		/* else */
			/****X*X*X*** SECURITY VIOLATION! RED ALERT, SHIELDS UP! ***X*X*X****\
			|* (Pasky was apparently looking at the Links2 JS code   .  ___ ^.^ *|
			\* for too long.)                                        `.(,_,)\o/ */
		break;
	}
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return true in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		break;
	}

	return true;
}

void location_goto(struct document_view *doc_view, unsigned char *url);

static bool window_alert(JSContext *ctx, unsigned int argc, jsval *rval);
static bool window_open(JSContext *ctx, unsigned int argc, jsval *rval);
static bool window_setTimeout(JSContext *ctx, unsigned int argc, jsval *rval);

const spidermonkeyFunctionSpec window_funcs[] = {
	{ "alert",	window_alert,		1 },
	{ "open",	window_open,		3 },
	{ "setTimeout",	window_setTimeout,	2 },
	{ NULL }
};

/* @window_funcs{"alert"} */
static bool
window_alert(JSContext *ctx, unsigned int argc, jsval *rval)
{
	jsval val;
	JSObject *obj = JS_THIS_OBJECT(ctx, rval);
	JS::RootedObject hobj(ctx, obj);
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
//	jsval *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	unsigned char *string;

	if (!JS_InstanceOf(ctx, hobj, &window_class, &args)) return false;

	vs = JS_GetInstancePrivate(ctx, hobj, &window_class, &args);

	if (argc != 1)
		return true;

	string = jsval_to_string(ctx, args[0].address());
	if (!*string)
		return true;

	info_box(vs->doc_view->session->tab->term, MSGBOX_FREE_TEXT,
		N_("JavaScript Alert"), ALIGN_CENTER, stracpy(string));

	args.rval().setUndefined();
	return true;
}

/* @window_funcs{"open"} */
static bool
window_open(JSContext *ctx, unsigned int argc, jsval *rval)
{
	jsval val;
	JSObject *obj = JS_THIS_OBJECT(ctx, rval);
	JS::RootedObject hobj(ctx, obj);
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
//	jsval *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	unsigned char *frame = NULL;
	unsigned char *url, *url2;
	struct uri *uri;
	static time_t ratelimit_start;
	static int ratelimit_count;

	if (!JS_InstanceOf(ctx, hobj, &window_class, &args)) return false;

	vs = JS_GetInstancePrivate(ctx, hobj, &window_class, &args);
	doc_view = vs->doc_view;
	ses = doc_view->session;

	if (get_opt_bool("ecmascript.block_window_opening", ses)) {
#ifdef CONFIG_LEDS
		set_led_value(ses->status.popup_led, 'P');
#endif
		return true;
	}

	if (argc < 1) return true;

	/* Ratelimit window opening. Recursive window.open() is very nice.
	 * We permit at most 20 tabs in 2 seconds. The ratelimiter is very
	 * rough but shall suffice against the usual cases. */
	if (!ratelimit_start || time(NULL) - ratelimit_start > 2) {
		ratelimit_start = time(NULL);
		ratelimit_count = 0;
	} else {
		ratelimit_count++;
		if (ratelimit_count > 20) {
			return true;
		}
	}

	url = stracpy(jsval_to_string(ctx, args[0].address()));
	trim_chars(url, ' ', 0);
	url2 = join_urls(doc_view->document->uri, url);
	mem_free(url);
	if (!url2) {
		return true;
	}
	if (argc > 1) {
		frame = stracpy(jsval_to_string(ctx, args[1].address()));
		if (!frame) {
			mem_free(url2);
			return true;
		}
	}

	/* TODO: Support for window naming and perhaps some window features? */

	uri = get_uri(url2, 0);
	mem_free(url2);
	if (!uri) {
		mem_free_if(frame);
		return true;
	}

	if (frame && *frame && c_strcasecmp(frame, "_blank")) {
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			deo->target = stracpy(frame);
			register_bottom_half(delayed_goto_uri_frame, deo);
			boolean_to_jsval(ctx, &val, 1);
			goto end;
		}
	}

	if (!get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home")
	    && !get_cmd_opt_bool("anonymous")
	    && can_open_in_new(ses->tab->term)) {
		open_uri_in_new_window(ses, uri, NULL, ENV_ANY,
				       CACHE_MODE_NORMAL, TASK_NONE);
		boolean_to_jsval(ctx, &val, 1);
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half(delayed_open, deo);
			boolean_to_jsval(ctx, &val, 1);
		} else {
			undef_to_jsval(ctx, &val);
		}
	}

end:
	done_uri(uri);
	mem_free_if(frame);

	args.rval().set(val);
	return true;
}

/* @window_funcs{"setTimeout"} */
static bool
window_setTimeout(JSContext *ctx, unsigned int argc, jsval *rval)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
//	jsval *argv = JS_ARGV(ctx, rval);
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	unsigned char *code;
	int timeout;

	if (argc != 2)
		return true;

	code = jsval_to_string(ctx, args[0].address());
	if (!*code)
		return true;

	code = stracpy(code);
	if (!code)
		return true;
	timeout = args[1].toInt32();
	if (timeout <= 0) {
		mem_free(code);
		return true;
	}
	ecmascript_set_timeout(interpreter, code, timeout);
	return true;
}

#if 0
static bool
window_get_property_closed(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &window_class, NULL))
		return false;

	boolean_to_jsval(ctx, vp, 0);

	return true;
}
#endif

static bool
window_get_property_closed(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setBoolean(false);

	return true;
}

static bool
window_get_property_parent(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);

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
	args.rval().setUndefined();

	return true;
}

static bool
window_get_property_self(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setObject(args.thisv().toObject());

	return true;
}

static bool
window_get_property_status(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setUndefined();

	return true;
}

static bool
window_set_property_status(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	if (args.length() != 1) {
		return false;
	}

	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs = JS_GetInstancePrivate(ctx, hobj, &window_class, NULL);

	if (!vs) {
		return true;
	}

	mem_free_set(&vs->doc_view->session->status.window_status, stracpy(JS_EncodeString(ctx, args[0].toString())));
	print_screen_status(vs->doc_view->session);

	return true;
}

static bool
window_get_property_top(JSContext *ctx, unsigned int argc, jsval *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document_view *top_view;
	JSObject *newjsframe;

	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	vs = JS_GetInstancePrivate(ctx, hobj, &window_class, NULL);

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	top_view = doc_view->session->doc_view;

	assert(top_view && top_view->vs);
	if (top_view->vs->ecmascript_fragile)
		ecmascript_reset_state(top_view->vs);
	if (!top_view->vs->ecmascript) {
		args.rval().setUndefined();
		return true;
	}
	newjsframe = JS::CurrentGlobalOrNull(top_view->vs->ecmascript->backend_data);

	/* Keep this unrolled this way. Will have to check document.domain
	 * JS property. */
	/* Note that this check is perhaps overparanoid. If top windows
	 * is alien but some other child window is not, we should still
	 * let the script walk thru. That'd mean moving the check to
	 * other individual properties in this switch. */
	if (compare_uri(vs->uri, top_view->vs->uri, URI_HOST)) {
		args.rval().setObject(*newjsframe);
		return true;
	}
		/* else */
		/****X*X*X*** SECURITY VIOLATION! RED ALERT, SHIELDS UP! ***X*X*X****\
		|* (Pasky was apparently looking at the Links2 JS code   .  ___ ^.^ *|
		\* for too long.)                                        `.(,_,)\o/ */
	args.rval().setUndefined();
	return true;
}
