/* Exports struct session to the world of ECMAScript */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include "main/select.h"
#include "protocol/uri.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/global_object.h"
#include "scripting/smjs/session_object.h"
#include "scripting/smjs/view_state_object.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "util/error.h"
#include "util/memory.h"
#include "viewer/text/vs.h"


static JSObject *smjs_session_object;

static const JSClass session_class; /* Defined below. */

static const JSClass location_array_class; /* Defined below. */

/* location_array_class is the class for array object, the elements of which
 * correspond to the elements of session.history.
 *
 * session_class.history returns a location_array_class object, so define
 * location_array_class and related routines before session_class. */

/* @location_array.getProperty */
static JSBool
smjs_location_array_get_property(JSContext *ctx, JSObject *obj, jsid id, jsval *vp)
{
	struct session *ses;
	int index;
	struct location *loc;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &location_array_class, NULL))
		return JS_FALSE;

	ses = JS_GetInstancePrivate(ctx, obj,
	                            (JSClass *) &location_array_class, NULL);
	if (!ses) return JS_FALSE;

	undef_to_jsval(ctx, vp);

	if (!JSID_IS_INT(id))
		return JS_FALSE;

	assert(ses);
	if_assert_failed return JS_TRUE;

	if (!have_location(ses)) return JS_FALSE;

	index = JSID_TO_INT(id);
	for (loc = cur_loc(ses);
	     loc != (struct location *) &ses->history.history;
	     loc = index > 0 ? loc->next : loc->prev) {
		if (!index) {
			JSObject *obj = smjs_get_view_state_object(&loc->vs);

			if (obj) object_to_jsval(ctx, vp, obj);

			return JS_TRUE;
		}

		index += index > 0 ? -1 : 1;
	}

	return JS_FALSE;
}

/** Pointed to by location_array_class.finalize.  SpiderMonkey automatically
 * finalizes all objects before it frees the JSRuntime, so
 * session.history_jsobject won't be left dangling.  */
static void
smjs_location_array_finalize(JSContext *ctx, JSObject *obj)
{
	struct session *ses;

	assert(JS_InstanceOf(ctx, obj, (JSClass *) &location_array_class, NULL));
	if_assert_failed return;

	ses = JS_GetInstancePrivate(ctx, obj,
	                            (JSClass *) &location_array_class, NULL);

	if (!ses) return; /* already detached */

	JS_SetPrivate(ctx, obj, NULL); /* perhaps not necessary */
	assert(ses->history_jsobject == obj);
	if_assert_failed return;
	ses->history_jsobject = NULL;
}

static const JSClass location_array_class = {
	"location_array",
	JSCLASS_HAS_PRIVATE, /* struct session *; a weak reference */
	JS_PropertyStub, JS_PropertyStub,
	smjs_location_array_get_property, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, smjs_location_array_finalize,
};

/** Return an SMJS object through which scripts can access @a ses.history.  If
 * there already is such an object, return that; otherwise create a new one.
 * The SMJS object holds only a weak reference to @a ses.  */
JSObject *
smjs_get_session_location_array_object(struct session *ses)
{
	JSObject *obj;

	if (ses->history_jsobject) return ses->history_jsobject;

	assert(smjs_ctx);
	if_assert_failed return NULL;

	obj = JS_NewObject(smjs_ctx, (JSClass *) &location_array_class, NULL, NULL);
	if (!obj) return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @ses.  */
	if (JS_FALSE == JS_SetPrivate(smjs_ctx, obj, ses))
		return NULL;

	ses->history_jsobject = obj;
	return obj;
}

/* There is no smjs_detach_session_location_array_object because
 * smjs_detach_session_object detaches both session.jsobject and
 * session.history.js_object. */


enum session_prop {
	SESSION_VISITED,
	SESSION_HISTORY,
	SESSION_LOADING_URI,
	SESSION_RELOADLEVEL,
	SESSION_REDIRECT_CNT,
	/* XXX: SESSION_DOC_VIEW, */
	/* XXX: SESSION_FRAMES, */
	SESSION_SEARCH_DIRECTION,
	SESSION_KBDPREFIX,
	SESSION_MARK_WAITING_FOR,
	SESSION_EXIT_QUERY,
	SESSION_INSERT_MODE,
	SESSION_NAVIGATE_MODE,
	SESSION_SEARCH_WORD,
	SESSION_LAST_SEARCH_WORD,
	/* XXX: SESSION_TYPE_QUERIES, */
};

static const JSPropertySpec session_props[] = {
	{ "visited",          SESSION_VISITED,          JSPROP_ENUMERATE },
	{ "history",          SESSION_HISTORY,          JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "loading_uri",      SESSION_LOADING_URI,      JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "reloadlevel",      SESSION_RELOADLEVEL,      JSPROP_ENUMERATE },
	{ "redirect_cnt",     SESSION_REDIRECT_CNT,     JSPROP_ENUMERATE },
	/* XXX: { "doc_view",         SESSION_DOC_VIEW,         JSPROP_ENUMERATE | JSPROP_READONLY }, */
	/* XXX: { "frames",           SESSION_FRAMES,           JSPROP_ENUMERATE | JSPROP_READONLY }, */
	{ "search_direction", SESSION_SEARCH_DIRECTION, JSPROP_ENUMERATE },
	{ "kbdprefix",        SESSION_KBDPREFIX,        JSPROP_ENUMERATE },
	{ "mark",             SESSION_MARK_WAITING_FOR, JSPROP_ENUMERATE },
	{ "exit_query",       SESSION_EXIT_QUERY,       JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "insert_mode",      SESSION_INSERT_MODE,      JSPROP_ENUMERATE },
	{ "navigate_mode",    SESSION_NAVIGATE_MODE,    JSPROP_ENUMERATE },
	{ "search_word",      SESSION_SEARCH_WORD,      JSPROP_ENUMERATE },
	{ "last_search_word", SESSION_LAST_SEARCH_WORD, JSPROP_ENUMERATE },
	/* XXX: { "type_queries",     SESSION_TYPE_QUERIES,     JSPROP_ENUMERATE }, */
	{ NULL }
};

/* @session_class.getProperty */
static JSBool
session_get_property(JSContext *ctx, JSObject *obj, jsid id, jsval *vp)
{
	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &session_class, NULL))
		return JS_FALSE;

	ses = JS_GetInstancePrivate(ctx, obj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return JS_FALSE;

	if (!JSID_IS_INT(id)) {
		/* Note: If we return JS_FALSE here, the object's methods do not
		 * work. */
		return JS_TRUE;
	}

	/* XXX: Lock session here if it is ever changed to have an OBJECT_HEAD. */

	undef_to_jsval(ctx, vp);

	switch (JSID_TO_INT(id)) {
	case SESSION_VISITED:
		int_to_jsval(ctx, vp, ses->status.visited);

		return JS_TRUE;
	case SESSION_HISTORY: {
		JSObject *obj = smjs_get_session_location_array_object(ses);

		if (obj) object_to_jsval(ctx, vp, obj);

		return JS_TRUE;
	}
	case SESSION_LOADING_URI: {
		struct uri *uri = have_location(ses) ? cur_loc(ses)->vs.uri
		                                     : ses->loading_uri;

		if (uri) string_to_jsval(ctx, vp, struri(uri));

		return JS_TRUE;
	}
	case SESSION_RELOADLEVEL:
		int_to_jsval(ctx, vp, ses->reloadlevel);

		return JS_TRUE;
	case SESSION_REDIRECT_CNT:
		int_to_jsval(ctx, vp, ses->redirect_cnt);

		return JS_TRUE;
	case SESSION_SEARCH_DIRECTION:
		string_to_jsval(ctx, vp, ses->search_direction == 1 ? "down"
		                                                    : "up");

		return JS_TRUE;
	case SESSION_KBDPREFIX:
		int_to_jsval(ctx, vp, ses->kbdprefix.repeat_count);

		return JS_TRUE;
	case SESSION_MARK_WAITING_FOR:
		string_to_jsval(ctx, vp, ses->kbdprefix.mark == KP_MARK_NOTHING
		                          ? "nothing"
		                          : ses->kbdprefix.mark == KP_MARK_SET
					     ? "set"
		                             : "goto");

		return JS_TRUE;
	case SESSION_EXIT_QUERY:
		int_to_jsval(ctx, vp, ses->exit_query);

		return JS_TRUE;
	case SESSION_INSERT_MODE:
		string_to_jsval(ctx, vp,
		                ses->insert_mode == INSERT_MODE_LESS
		                 ? "disabled"
		                 : ses->insert_mode == INSERT_MODE_ON
		                    ? "on"
		                    : "off");

		return JS_TRUE;
	case SESSION_NAVIGATE_MODE:
		string_to_jsval(ctx, vp,
		                ses->navigate_mode == NAVIGATE_CURSOR_ROUTING
		                 ? "cursor"
		                 : "linkwise");

		return JS_TRUE;
	case SESSION_SEARCH_WORD:
		string_to_jsval(ctx, vp, ses->search_word);

		return JS_TRUE;
	case SESSION_LAST_SEARCH_WORD:
		string_to_jsval(ctx, vp, ses->last_search_word);

		return JS_TRUE;
	default:
		INTERNAL("Invalid ID %d in session_get_property().",
		         JSID_TO_INT(id));
	}

	return JS_FALSE;
}

static JSBool
session_set_property(JSContext *ctx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &session_class, NULL))
		return JS_FALSE;

	ses = JS_GetInstancePrivate(ctx, obj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return JS_FALSE;

	if (!JSID_IS_INT(id))
		return JS_FALSE;

	switch (JSID_TO_INT(id)) {
	case SESSION_VISITED:
		ses->status.visited = atol(jsval_to_string(ctx, vp));

		return JS_TRUE;
	/* SESSION_HISTORY is RO */
	/* SESSION_LOADING_URI is RO */
	case SESSION_RELOADLEVEL:
		ses->reloadlevel = atol(jsval_to_string(ctx, vp));

		return JS_TRUE;
	case SESSION_REDIRECT_CNT:
		ses->redirect_cnt = atol(jsval_to_string(ctx, vp));

		return JS_TRUE;
	case SESSION_SEARCH_DIRECTION: {
		unsigned char *str;
		JSString *jsstr;

		jsstr = JS_ValueToString(ctx, *vp);
		if (!jsstr) return JS_TRUE;

		str = JS_EncodeString(ctx, jsstr);
		if (!str) return JS_TRUE;

		if (!strcmp(str, "up"))
			ses->search_direction = -1;
		else if (!strcmp(str, "down"))
			ses->search_direction = 1;
		else
			return JS_FALSE;

		return JS_TRUE;
	}
	case SESSION_KBDPREFIX:
		ses->kbdprefix.repeat_count = atol(jsval_to_string(ctx, vp));

		return JS_TRUE;
	case SESSION_MARK_WAITING_FOR: {
		unsigned char *str;
		JSString *jsstr;

		jsstr = JS_ValueToString(ctx, *vp);
		if (!jsstr) return JS_TRUE;

		str = JS_EncodeString(ctx, jsstr);
		if (!str) return JS_TRUE;

		if (!strcmp(str, "nothing"))
			ses->kbdprefix.mark = KP_MARK_NOTHING;
		else if (!strcmp(str, "set"))
			ses->kbdprefix.mark = KP_MARK_SET;
		else if (!strcmp(str, "goto"))
			ses->kbdprefix.mark = KP_MARK_GOTO;
		else
			return JS_FALSE;

		return JS_TRUE;
	}
	/* SESSION_EXIT_QUERY is RO */
	case SESSION_INSERT_MODE: {
		unsigned char *str;
		JSString *jsstr;

		jsstr = JS_ValueToString(ctx, *vp);
		if (!jsstr) return JS_TRUE;

		str = JS_EncodeString(ctx, jsstr);
		if (!str) return JS_TRUE;

		if (!strcmp(str, "disabled"))
			ses->insert_mode = INSERT_MODE_LESS;
		else if (!strcmp(str, "on"))
			ses->insert_mode = INSERT_MODE_ON;
		else if (!strcmp(str, "off"))
			ses->insert_mode = INSERT_MODE_OFF;
		else
			return JS_FALSE;

		return JS_TRUE;
	}
	case SESSION_NAVIGATE_MODE: {
		unsigned char *str;
		JSString *jsstr;

		jsstr = JS_ValueToString(ctx, *vp);
		if (!jsstr) return JS_TRUE;

		str = JS_EncodeString(ctx, jsstr);
		if (!str) return JS_TRUE;

		if (!strcmp(str, "cursor"))
			ses->navigate_mode = NAVIGATE_CURSOR_ROUTING;
		else if (!strcmp(str, "linkwise"))
			ses->navigate_mode = NAVIGATE_LINKWISE;
		else
			return JS_FALSE;

		return JS_TRUE;
	}
	case SESSION_SEARCH_WORD: {
		unsigned char *str;
		JSString *jsstr;

		jsstr = JS_ValueToString(ctx, *vp);
		if (!jsstr) return JS_TRUE;

		str = JS_EncodeString(ctx, jsstr);
		if (!str) return JS_TRUE;

		mem_free_set(&ses->search_word, str);

		return JS_TRUE;
	}
	case SESSION_LAST_SEARCH_WORD: {
		unsigned char *str;
		JSString *jsstr;

		jsstr = JS_ValueToString(ctx, *vp);
		if (!jsstr) return JS_TRUE;

		str = JS_EncodeString(ctx, jsstr);
		if (!str) return JS_TRUE;

		mem_free_set(&ses->last_search_word, str);

		return JS_TRUE;
	}
	default:
		INTERNAL("Invalid ID %d in session_set_property().",
		         JSID_TO_INT(id));
	}

	return JS_FALSE;
}

/** Pointed to by session_class.construct.  Create a new session (tab)
 * and return the JSObject wrapper.  */
static JSBool
session_construct(JSContext *ctx, uintN argc, jsval *rval)
{
	jsval val;
	jsval *argv = JS_ARGV(ctx, rval);
	int bg = 0; /* open new tab in background */
	struct session *ses;
	JSObject *jsobj;

	if (argc > 1) {
		return JS_TRUE;
	}

	if (argc >= 1) {
		bg = jsval_to_boolean(ctx, &argv[0]);
	}

	if (!smjs_ses) return JS_FALSE;

	ses = init_session(smjs_ses, smjs_ses->tab->term, NULL, bg);
	if (!ses) return JS_FALSE;

	jsobj = smjs_get_session_object(ses);
	if (!jsobj) return JS_FALSE;

	object_to_jsval(ctx, &val, jsobj);
	JS_SET_RVAL(ctx, rval, val);

	return JS_TRUE;
}

/** Pointed to by session_class.finalize.  SpiderMonkey automatically
 * finalizes all objects before it frees the JSRuntime, so session.jsobject
 * won't be left dangling.  */
static void
session_finalize(JSContext *ctx, JSObject *obj)
{
	struct session *ses;

	assert(JS_InstanceOf(ctx, obj, (JSClass *) &session_class, NULL));
	if_assert_failed return;

	ses = JS_GetInstancePrivate(ctx, obj,
	                            (JSClass *) &session_class, NULL);

	if (!ses) return; /* already detached */

	JS_SetPrivate(ctx, obj, NULL); /* perhaps not necessary */
	assert(ses->jsobject == obj);
	if_assert_failed return;
	ses->jsobject = NULL;
}

static const JSClass session_class = {
	"session",
	JSCLASS_HAS_PRIVATE, /* struct session *; a weak reference */
	JS_PropertyStub, JS_PropertyStub,
	session_get_property, session_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, session_finalize,
	NULL, NULL, session_construct
};

/** Return an SMJS object through which scripts can access @a ses.
 * If there already is such an object, return that; otherwise create a
 * new one.  The SMJS object holds only a weak reference to @a ses.  */
JSObject *
smjs_get_session_object(struct session *ses)
{
	JSObject *obj;

	if (ses->jsobject) return ses->jsobject;

	assert(smjs_ctx);
	if_assert_failed return NULL;

	obj = JS_NewObject(smjs_ctx, (JSClass *) &session_class, NULL, NULL);
	if (!obj) return NULL;

	if (JS_FALSE == JS_DefineProperties(smjs_ctx, obj,
	                               (JSPropertySpec *) session_props))
		return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @ses.  */
	if (JS_FALSE == JS_SetPrivate(smjs_ctx, obj, ses)) /* to @session_class */
		return NULL;

	ses->jsobject = obj;
	return obj;
}

/** Ensure that no JSObject contains the pointer @a ses.  This is
 * called from destroy_session before @a ses is freed.  If a JSObject was
 * previously attached to the session object, the object will remain in
 * memory but it will no longer be able to access the session object. */
void
smjs_detach_session_object(struct session *ses)
{
	assert(smjs_ctx);
	assert(ses);
	if_assert_failed return;

	if (ses->jsobject) {
		assert(JS_GetInstancePrivate(smjs_ctx, ses->jsobject,
					     (JSClass *) &session_class, NULL)
		       == ses);
		if_assert_failed {}

		JS_SetPrivate(smjs_ctx, ses->jsobject, NULL);
		ses->jsobject = NULL;
	}

	if (ses->history_jsobject) {
		assert(JS_GetInstancePrivate(smjs_ctx, ses->history_jsobject,
					     (JSClass *) &location_array_class,
		                             NULL)
		       == ses);
		if_assert_failed {}

		JS_SetPrivate(smjs_ctx, ses->history_jsobject, NULL);
		ses->history_jsobject = NULL;
	}
}


/** Ensure that no JSObject contains the pointer @a ses.  This is
 * called when the reference count of the session object *@a ses is
 * already 0 and it is about to be freed.  If a JSObject was
 * previously attached to the session object, the object will remain in
 * memory but it will no longer be able to access the session object. */
static JSBool
session_array_get_property(JSContext *ctx, JSObject *obj, jsid id, jsval *vp)
{
	JSObject *tabobj;
	struct terminal *term = JS_GetPrivate(ctx, obj);
	int index;
	struct window *tab;

	undef_to_jsval(ctx, vp);

	if (!JSID_IS_INT(id))
		return JS_FALSE;

	assert(term);
	if_assert_failed return JS_TRUE;

	index  = JSID_TO_INT(id);
	foreach_tab (tab, term->windows) {
		if (!index) break;
		--index;
	}
	if ((void *) tab == (void *) &term->windows) return JS_FALSE;

	tabobj = smjs_get_session_object(tab->data);
	if (tabobj) object_to_jsval(ctx, vp, tabobj);

	return JS_TRUE;
}

static const JSClass session_array_class = {
	"session_array",
	JSCLASS_HAS_PRIVATE, /* struct terminal *term; a weak reference */
	JS_PropertyStub, JS_PropertyStub,
	session_array_get_property, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

JSObject *
smjs_get_session_array_object(struct terminal *term)
{
	JSObject *obj;

	assert(smjs_ctx);
	if_assert_failed return NULL;

	obj = JS_NewObject(smjs_ctx, (JSClass *) &session_array_class,
	                   NULL, NULL);
	if (!obj) return NULL;

	if (JS_FALSE == JS_SetPrivate(smjs_ctx, obj, term))
		return NULL;

	return obj;
}

/** Ensure that no JSObject contains the pointer @a term.  This is called from
 * smjs_detach_terminal_object.  If a JSObject was previously attached to the
 * terminal object, the object will remain in memory but it will no longer be
 * able to access the terminal object. */
void
smjs_detach_session_array_object(struct terminal *term)
{
	assert(smjs_ctx);
	assert(term);
	if_assert_failed return;

	if (!term->session_array_jsobject) return;

	assert(JS_GetInstancePrivate(smjs_ctx, term->session_array_jsobject,
				     (JSClass *) &session_array_class, NULL)
	       == term);
	if_assert_failed {}

	JS_SetPrivate(smjs_ctx, term->session_array_jsobject, NULL);
	term->session_array_jsobject = NULL;
}

static JSBool
smjs_session_goto_url(JSContext *ctx, uintN argc, jsval *rval)
{
	jsval val;
	struct delayed_open *deo;
	struct uri *uri;
	jsval *argv = JS_ARGV(ctx, rval);
	JSString *jsstr;
	unsigned char *url;
	struct session *ses;
	struct JSObject *this;

	if (argc != 1) return JS_FALSE;

	this = JS_THIS_OBJECT(ctx, rval);
	if (!JS_InstanceOf(ctx, this, (JSClass *) &session_class, NULL))
		return JS_FALSE;

	ses = JS_GetInstancePrivate(ctx, this,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return JS_FALSE; /* detached */

	jsstr = JS_ValueToString(ctx, argv[0]);
	if (!jsstr) return JS_FALSE;

	url = JS_EncodeString(ctx, jsstr);
	if (!url) return JS_FALSE;

	uri = get_uri(url, 0);
	if (!uri) return JS_FALSE;

	deo = mem_calloc(1, sizeof(*deo));
	if (!deo) {
		done_uri(uri);
		return JS_FALSE;
	}

	deo->ses = ses;
	deo->uri = get_uri_reference(uri);
	/* deo->target = NULL; */
	register_bottom_half(delayed_goto_uri_frame, deo);

	undef_to_jsval(ctx, &val);
	JS_SET_RVAL(ctx, rval, val);

	done_uri(uri);

	return JS_TRUE;
}

static const spidermonkeyFunctionSpec session_funcs[] = {
	{ "goto",    smjs_session_goto_url, 1 },
	{ NULL }
};

void
smjs_init_session_interface(void)
{
	assert(smjs_ctx);
	assert(smjs_global_object);

	smjs_session_object =
	 spidermonkey_InitClass(smjs_ctx, smjs_global_object, NULL,
	                        (JSClass *) &session_class, session_construct,
	                        0, (JSPropertySpec *) session_props,
	                        session_funcs, NULL, NULL);
}
