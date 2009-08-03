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


static JSBool window_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool window_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

const JSClass window_class = {
	"window",
	JSCLASS_HAS_PRIVATE,	/* struct view_state * */
	JS_PropertyStub, JS_PropertyStub,
	window_get_property, window_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
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
const JSPropertySpec window_props[] = {
	{ "closed",	JSP_WIN_CLOSED,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "parent",	JSP_WIN_PARENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "self",	JSP_WIN_SELF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "status",	JSP_WIN_STATUS,	JSPROP_ENUMERATE },
	{ "top",	JSP_WIN_TOP,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "window",	JSP_WIN_SELF,	JSPROP_ENUMERATE | JSPROP_READONLY },
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
	return JS_GetGlobalObject(target->vs.ecmascript->backend_data);
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
static JSBool
window_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &window_class, NULL))
		return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, obj, (JSClass *) &window_class, NULL);

	/* No need for special window.location measurements - when
	 * location is then evaluated in string context, toString()
	 * is called which we overrode for that class below, so
	 * everything's fine. */
	if (JSVAL_IS_STRING(id)) {
		struct document_view *doc_view = vs->doc_view;
		JSObject *obj;

		obj = try_resolve_frame(doc_view, jsval_to_string(ctx, &id));
		/* TODO: Try other lookups (mainly element lookup) until
		 * something yields data. */
		if (obj) {
			object_to_jsval(ctx, vp, obj);
		}
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
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
		object_to_jsval(ctx, vp, JS_GetGlobalObject(doc_view->ecmascript->backend_data));
		break;
	}
#endif
	case JSP_WIN_STATUS:
		return JS_FALSE;
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
		newjsframe = JS_GetGlobalObject(top_view->vs->ecmascript->backend_data);

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
		 * js_RegExpClass) just return JS_TRUE in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		break;
	}

	return JS_TRUE;
}

/* @window_class.setProperty */
static JSBool
window_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct view_state *vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &window_class, NULL))
		return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, obj, (JSClass *) &window_class, NULL);

	if (JSVAL_IS_STRING(id)) {
		if (!strcmp(jsval_to_string(ctx, &id), "location")) {
			struct document_view *doc_view = vs->doc_view;

			location_goto(doc_view, jsval_to_string(ctx, vp));
			/* Do NOT touch our .location property, evil
			 * SpiderMonkey!! */
			return JS_FALSE;
		}
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_WIN_STATUS:
		mem_free_set(&vs->doc_view->session->status.window_status, stracpy(jsval_to_string(ctx, vp)));
		print_screen_status(vs->doc_view->session);
		return JS_TRUE;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case.
		 * Do the same here.  */
		return JS_TRUE;
	}

	return JS_TRUE;
}


static JSBool window_alert(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool window_open(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool window_setTimeout(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

const spidermonkeyFunctionSpec window_funcs[] = {
	{ "alert",	window_alert,		1 },
	{ "open",	window_open,		3 },
	{ "setTimeout",	window_setTimeout,	2 },
	{ NULL }
};

/* @window_funcs{"alert"} */
static JSBool
window_alert(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct view_state *vs;
	unsigned char *string;

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &window_class, argv)) return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, obj, (JSClass *) &window_class, argv);

	if (argc != 1)
		return JS_TRUE;

	string = jsval_to_string(ctx, &argv[0]);
	if (!*string)
		return JS_TRUE;

	info_box(vs->doc_view->session->tab->term, MSGBOX_FREE_TEXT,
		N_("JavaScript Alert"), ALIGN_CENTER, stracpy(string));

	undef_to_jsval(ctx, rval);
	return JS_TRUE;
}

/* @window_funcs{"open"} */
static JSBool
window_open(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	unsigned char *frame = NULL;
	unsigned char *url, *url2;
	struct uri *uri;
	static time_t ratelimit_start;
	static int ratelimit_count;

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &window_class, argv)) return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, obj, (JSClass *) &window_class, argv);
	doc_view = vs->doc_view;
	ses = doc_view->session;

	if (get_opt_bool("ecmascript.block_window_opening")) {
#ifdef CONFIG_LEDS
		set_led_value(ses->status.popup_led, 'P');
#endif
		return JS_TRUE;
	}

	if (argc < 1) return JS_TRUE;

	/* Ratelimit window opening. Recursive window.open() is very nice.
	 * We permit at most 20 tabs in 2 seconds. The ratelimiter is very
	 * rough but shall suffice against the usual cases. */
	if (!ratelimit_start || time(NULL) - ratelimit_start > 2) {
		ratelimit_start = time(NULL);
		ratelimit_count = 0;
	} else {
		ratelimit_count++;
		if (ratelimit_count > 20) {
			return JS_TRUE;
		}
	}

	url = stracpy(jsval_to_string(ctx, &argv[0]));
	trim_chars(url, ' ', 0);
	url2 = join_urls(doc_view->document->uri, url);
	mem_free(url);
	if (!url2) {
		return JS_TRUE;
	}
	if (argc > 1) {
		frame = stracpy(jsval_to_string(ctx, &argv[1]));
		if (!frame) {
			mem_free(url2);
			return JS_TRUE;
		}
	}

	/* TODO: Support for window naming and perhaps some window features? */

	uri = get_uri(url2, 0);
	mem_free(url2);
	if (!uri) {
		mem_free_if(frame);
		return JS_TRUE;
	}

	if (frame && *frame && c_strcasecmp(frame, "_blank")) {
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			deo->target = stracpy(frame);
			register_bottom_half(delayed_goto_uri_frame, deo);
			boolean_to_jsval(ctx, rval, 1);
			goto end;
		}
	}

	if (!get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home")
	    && !get_cmd_opt_bool("anonymous")
	    && can_open_in_new(ses->tab->term)) {
		open_uri_in_new_window(ses, uri, NULL, ENV_ANY,
				       CACHE_MODE_NORMAL, TASK_NONE);
		boolean_to_jsval(ctx, rval, 1);
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half(delayed_open, deo);
			boolean_to_jsval(ctx, rval, 1);
		} else {
			undef_to_jsval(ctx, rval);
		}
	}

end:
	done_uri(uri);
	mem_free_if(frame);

	return JS_TRUE;
}

/* @window_funcs{"setTimeout"} */
static JSBool
window_setTimeout(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	unsigned char *code;
	int timeout;

	if (argc != 2)
		return JS_TRUE;

	code = jsval_to_string(ctx, &argv[0]);
	if (!*code)
		return JS_TRUE;

	code = stracpy(code);
	if (!code)
		return JS_TRUE;
	timeout = atoi(jsval_to_string(ctx, &argv[1]));
	if (timeout <= 0) {
		mem_free(code);
		return JS_TRUE;
	}
	ecmascript_set_timeout(interpreter, code, timeout);
	return JS_TRUE;
}
