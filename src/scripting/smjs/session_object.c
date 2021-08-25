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

static bool session_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool session_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static void session_finalize(JSFreeOp *op, JSObject *obj);
static bool session_construct(JSContext *ctx, unsigned int argc, JS::Value *rval);

static const JSClassOps session_ops = {
	nullptr, nullptr,
	session_get_property, session_set_property,
	nullptr, nullptr, nullptr, session_finalize,
	NULL, NULL, NULL, session_construct
};

static const JSClass session_class = {
	"session",
	JSCLASS_HAS_PRIVATE, /* struct session *; a weak reference */
	&session_ops
};

static bool smjs_location_array_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static void smjs_location_array_finalize(JSFreeOp *op, JSObject *obj);

static const JSClassOps location_array_ops = {
	nullptr, nullptr,
	smjs_location_array_get_property, nullptr,
	nullptr, nullptr, nullptr, smjs_location_array_finalize,
};

static const JSClass location_array_class = {
	"location_array",
	JSCLASS_HAS_PRIVATE, /* struct session *; a weak reference */
	&location_array_ops
};

/* location_array_class is the class for array object, the elements of which
 * correspond to the elements of session.history.
 *
 * session_class.history returns a location_array_class object, so define
 * location_array_class and related routines before session_class. */

/* @location_array.getProperty */
static bool
smjs_location_array_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	struct session *ses;
	int index;
	struct location *loc;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &location_array_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &location_array_class, NULL);
	if (!ses) return false;

	hvp.setUndefined();

	if (!JSID_IS_INT(id))
		return false;

	assert(ses);
	if_assert_failed return true;

	if (!have_location(ses)) return false;

	index = JSID_TO_INT(id);
	for (loc = cur_loc(ses);
	     loc != (struct location *) &ses->history.history;
	     loc = index > 0 ? loc->next : loc->prev) {
		if (!index) {
			JSObject *obj = smjs_get_view_state_object(&loc->vs);

			if (obj) {
				hvp.setObject(*obj);
			}

			return true;
		}

		index += index > 0 ? -1 : 1;
	}

	return false;
}

/** Pointed to by location_array_class.finalize.  SpiderMonkey automatically
 * finalizes all objects before it frees the JSRuntime, so
 * session.history_jsobject won't be left dangling.  */
static void
smjs_location_array_finalize(JSFreeOp *op, JSObject *obj)
{
	struct session *ses;

#if 0
	assert(JS_InstanceOf(ctx, obj, (JSClass *) &location_array_class, NULL));
	if_assert_failed return;
#endif

	ses = JS_GetPrivate(obj);

	if (!ses) return; /* already detached */

	JS_SetPrivate(obj, NULL); /* perhaps not necessary */
	assert(ses->history_jsobject == obj);
	if_assert_failed return;
	ses->history_jsobject = NULL;
}


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

	obj = JS_NewObject(smjs_ctx, (JSClass *) &location_array_class);
	if (!obj) return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @ses.  */
	JS_SetPrivate(obj, ses);

	ses->history_jsobject = obj;
	return obj;
}

/* There is no smjs_detach_session_location_array_object because
 * smjs_detach_session_object detaches both session.jsobject and
 * session.history.js_object. */

static bool session_get_property_visited(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_visited(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_history(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_loading_uri(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_reloadlevel(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_reloadlevel(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_redirect_cnt(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_redirect_cnt(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_search_direction(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_search_direction(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_kbdprefix(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_kbdprefix(JSContext *ctx, unsigned int argc, JS::Value *vp);

static bool session_get_property_mark(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_mark(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_exit_query(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_insert_mode(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_insert_mode(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_navigate_mode(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_navigate_mode(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_search_word(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_search_word(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_get_property_last_search_word(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool session_set_property_last_search_word(JSContext *ctx, unsigned int argc, JS::Value *vp);

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
	JS_PSGS("visited", session_get_property_visited, session_set_property_visited, JSPROP_ENUMERATE),
	JS_PSG("history", session_get_property_history, JSPROP_ENUMERATE),
	JS_PSG("loading_uri", session_get_property_loading_uri, JSPROP_ENUMERATE),
	JS_PSGS("reloadlevel", session_get_property_reloadlevel, session_set_property_reloadlevel, JSPROP_ENUMERATE),
	JS_PSGS("redirect_cnt", session_get_property_redirect_cnt, session_set_property_redirect_cnt, JSPROP_ENUMERATE),
	/* XXX: { "doc_view",         SESSION_DOC_VIEW,         JSPROP_ENUMERATE | JSPROP_READONLY }, */
	/* XXX: { "frames",           SESSION_FRAMES,           JSPROP_ENUMERATE | JSPROP_READONLY }, */
	JS_PSGS("search_direction", session_get_property_search_direction, session_set_property_search_direction, JSPROP_ENUMERATE),
	JS_PSGS("kbdprefix", session_get_property_kbdprefix, session_set_property_kbdprefix, JSPROP_ENUMERATE),
	JS_PSGS("mark", session_get_property_mark, session_set_property_mark, JSPROP_ENUMERATE),
	JS_PSG("exit_query", session_get_property_exit_query, JSPROP_ENUMERATE),
	JS_PSGS("insert_mode", session_get_property_insert_mode, session_set_property_insert_mode, JSPROP_ENUMERATE),
	JS_PSGS("navigate_mode", session_get_property_navigate_mode, session_set_property_navigate_mode, JSPROP_ENUMERATE),
	JS_PSGS("search_word", session_get_property_search_word, session_set_property_search_word, JSPROP_ENUMERATE),
	JS_PSGS("last_search_word", session_get_property_last_search_word, session_set_property_last_search_word, JSPROP_ENUMERATE),
	/* XXX: { "type_queries",     SESSION_TYPE_QUERIES,     JSPROP_ENUMERATE }, */
	JS_PS_END
};

static bool
session_get_property_visited(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setInt32(ses->status.visited);

	return true;
}

static bool
session_get_property_history(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	JSObject *obj = smjs_get_session_location_array_object(ses);

	if (obj) {
		args.rval().setObject(*obj);
	} else {
		args.rval().setUndefined();
	}

	return true;
}

static bool
session_get_property_loading_uri(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	struct uri *uri = have_location(ses) ? cur_loc(ses)->vs.uri
	                                     : ses->loading_uri;

	if (uri) {
		args.rval().setString(JS_NewStringCopyZ(ctx, struri(uri)));
	} else {
		args.rval().setUndefined();
	}

	return true;
}

static bool
session_get_property_reloadlevel(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setInt32(ses->reloadlevel);

	return true;
}

static bool
session_get_property_redirect_cnt(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setInt32(ses->redirect_cnt);

	return true;
}

static bool
session_get_property_search_direction(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setString(JS_NewStringCopyZ(ctx, ses->search_direction == 1 ? "down" : "up"));

	return true;
}

static bool
session_get_property_kbdprefix(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setInt32(ses->kbdprefix.repeat_count);

	return true;
}

static bool
session_get_property_mark(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setString(JS_NewStringCopyZ(ctx, ses->kbdprefix.mark == KP_MARK_NOTHING
		? "nothing"
		: ses->kbdprefix.mark == KP_MARK_SET
			? "set"
			: "goto"));

	return true;
}

static bool
session_get_property_exit_query(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setInt32(ses->exit_query);

	return true;
}

static bool
session_get_property_insert_mode(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setString(JS_NewStringCopyZ(ctx,
		ses->insert_mode == INSERT_MODE_LESS
		? "disabled"
		: ses->insert_mode == INSERT_MODE_ON
			? "on"
			: "off"));

	return true;
}

static bool
session_get_property_navigate_mode(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setString(JS_NewStringCopyZ(ctx,
		ses->navigate_mode == NAVIGATE_CURSOR_ROUTING
		? "cursor"
		: "linkwise"));

	return true;
}

static bool
session_get_property_search_word(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setString(JS_NewStringCopyZ(ctx, ses->search_word));

	return true;
}

static bool
session_get_property_last_search_word(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	args.rval().setString(JS_NewStringCopyZ(ctx, ses->last_search_word));

	return true;
}

/* @session_class.getProperty */
static bool
session_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	if (!JSID_IS_INT(id)) {
		/* Note: If we return false here, the object's methods do not
		 * work. */
		return true;
	}

	return false;
}

static bool
session_set_property_visited(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	int v = args[0].toInt32();
	ses->status.visited = v;

	return true;
}

static bool
session_set_property_reloadlevel(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	ses->reloadlevel = args[0].toInt32();

	return true;
}

static bool
session_set_property_redirect_cnt(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	ses->redirect_cnt = args[0].toInt32();

	return true;
}

static bool
session_set_property_search_direction(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	char *str;
	JSString *jsstr;

	jsstr = args[0].toString();
	if (!jsstr) return true;

	str = JS_EncodeString(ctx, jsstr);
	if (!str) return true;

	if (!strcmp(str, "up"))
		ses->search_direction = -1;
	else if (!strcmp(str, "down"))
		ses->search_direction = 1;
	else
		return false;

	return true;
}

static bool
session_set_property_kbdprefix(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	ses->kbdprefix.repeat_count = args[0].toInt32();

	return true;
}

static bool
session_set_property_mark(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	char *str;
	JSString *jsstr;

	jsstr = args[0].toString();
	if (!jsstr) return true;

	str = JS_EncodeString(ctx, jsstr);
	if (!str) return true;

	if (!strcmp(str, "nothing"))
		ses->kbdprefix.mark = KP_MARK_NOTHING;
	else if (!strcmp(str, "set"))
		ses->kbdprefix.mark = KP_MARK_SET;
	else if (!strcmp(str, "goto"))
		ses->kbdprefix.mark = KP_MARK_GOTO;
	else
		return false;

	return true;
}

static bool
session_set_property_insert_mode(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	char *str;
	JSString *jsstr;

	jsstr = args[0].toString();
	if (!jsstr) return true;

	str = JS_EncodeString(ctx, jsstr);
	if (!str) return true;

	if (!strcmp(str, "disabled"))
		ses->insert_mode = INSERT_MODE_LESS;
	else if (!strcmp(str, "on"))
		ses->insert_mode = INSERT_MODE_ON;
	else if (!strcmp(str, "off"))
		ses->insert_mode = INSERT_MODE_OFF;
	else
		return false;

	return true;
}

static bool
session_set_property_navigate_mode(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	char *str;
	JSString *jsstr;

	jsstr = args[0].toString();
	if (!jsstr) return true;

	str = JS_EncodeString(ctx, jsstr);
	if (!str) return true;

	if (!strcmp(str, "cursor"))
		ses->navigate_mode = NAVIGATE_CURSOR_ROUTING;
	else if (!strcmp(str, "linkwise"))
		ses->navigate_mode = NAVIGATE_LINKWISE;
	else
		return false;

	return true;
}

static bool
session_set_property_search_word(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	char *str;
	JSString *jsstr;

	jsstr = args[0].toString();
	if (!jsstr) return true;

	str = JS_EncodeString(ctx, jsstr);
	if (!str) return true;

	mem_free_set(&ses->search_word, str);

	return true;
}

static bool
session_set_property_last_search_word(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	char *str;
	JSString *jsstr;

	jsstr = args[0].toString();
	if (!jsstr) return true;

	str = JS_EncodeString(ctx, jsstr);
	if (!str) return true;

	mem_free_set(&ses->last_search_word, str);

	return true;
}



static bool
session_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	struct session *ses;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, hobj,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false;

	if (!JSID_IS_INT(id))
		return false;

	return false;
}

/** Pointed to by session_class.construct.  Create a new session (tab)
 * and return the JSObject wrapper.  */
static bool
session_construct(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::CallArgs args = CallArgsFromVp(argc, rval);
	//JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::Value val;
	int bg = 0; /* open new tab in background */
	struct session *ses;
	JSObject *jsobj;

	if (argc > 1) {
		return true;
	}

	if (argc >= 1) {
		bg = args[0].toBoolean();
	}

	if (!smjs_ses) return false;

	ses = init_session(smjs_ses, smjs_ses->tab->term, NULL, bg);
	if (!ses) return false;

	jsobj = smjs_get_session_object(ses);
	if (!jsobj) return false;

	args.rval().setObject(*jsobj);

	return true;
}

/** Pointed to by session_class.finalize.  SpiderMonkey automatically
 * finalizes all objects before it frees the JSRuntime, so session.jsobject
 * won't be left dangling.  */
static void
session_finalize(JSFreeOp *op, JSObject *obj)
{
	struct session *ses;

#if 0
	assert(JS_InstanceOf(ctx, obj, (JSClass *) &session_class, NULL));
	if_assert_failed return;
#endif

	ses = JS_GetPrivate(obj);

	if (!ses) return; /* already detached */

	JS_SetPrivate(obj, NULL); /* perhaps not necessary */
	assert(ses->jsobject == obj);
	if_assert_failed return;
	ses->jsobject = NULL;
}


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

	obj = JS_NewObject(smjs_ctx, (JSClass *) &session_class);
	if (!obj) return NULL;

	JS::RootedObject r_obj(smjs_ctx, obj);

	if (false == JS_DefineProperties(smjs_ctx, r_obj,
	                               (JSPropertySpec *) session_props))
		return NULL;

	/* Do this last, so that if any previous step fails, we can
	 * just forget the object and its finalizer won't attempt to
	 * access @ses.  */
	JS_SetPrivate(obj, ses); /* to @session_class */

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
		JS::RootedObject r_jsobject(smjs_ctx, ses->jsobject);
		assert(JS_GetInstancePrivate(smjs_ctx, r_jsobject,
					     (JSClass *) &session_class, NULL)
		       == ses);
		if_assert_failed {}

		JS_SetPrivate(ses->jsobject, NULL);
		ses->jsobject = NULL;
	}

	if (ses->history_jsobject) {
		JS::RootedObject r_history_jsobject(smjs_ctx, ses->history_jsobject);

		assert(JS_GetInstancePrivate(smjs_ctx, r_history_jsobject,
					     (JSClass *) &location_array_class,
		                             NULL)
		       == ses);
		if_assert_failed {}

		JS_SetPrivate(ses->history_jsobject, NULL);
		ses->history_jsobject = NULL;
	}
}


/** Ensure that no JSObject contains the pointer @a ses.  This is
 * called when the reference count of the session object *@a ses is
 * already 0 and it is about to be freed.  If a JSObject was
 * previously attached to the session object, the object will remain in
 * memory but it will no longer be able to access the session object. */
static bool
session_array_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS

	JSObject *tabobj;
	struct terminal *term = JS_GetPrivate(obj);
	int index;
	struct window *tab;

	hvp.setUndefined();

	if (!JSID_IS_INT(hid))
		return false;

	assert(term);
	if_assert_failed return true;

	index  = JSID_TO_INT(hid);
	foreach_tab (tab, term->windows) {
		if (!index) break;
		--index;
	}
	if ((void *) tab == (void *) &term->windows) return false;

	tabobj = smjs_get_session_object(tab->data);
	if (tabobj) {
		hvp.setObject(*tabobj);
	}

	return true;
}

static const JSClassOps session_array_ops = {
	nullptr, nullptr,
	session_array_get_property, nullptr,
	nullptr, nullptr, nullptr, nullptr
};

static const JSClass session_array_class = {
	"session_array",
	JSCLASS_HAS_PRIVATE, /* struct terminal *term; a weak reference */
	&session_array_ops
};

JSObject *
smjs_get_session_array_object(struct terminal *term)
{
	JSObject *obj;

	assert(smjs_ctx);
	if_assert_failed return NULL;

	obj = JS_NewObject(smjs_ctx, (JSClass *) &session_array_class);
	if (!obj) return NULL;

	JS_SetPrivate(obj, term);

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

	JS::RootedObject r_term_session_array_jsobject(smjs_ctx, term->session_array_jsobject);
	assert(JS_GetInstancePrivate(smjs_ctx, r_term_session_array_jsobject,
				     (JSClass *) &session_array_class, NULL)
	       == term);
	if_assert_failed {}

	JS_SetPrivate(term->session_array_jsobject, NULL);
	term->session_array_jsobject = NULL;
}

static bool
smjs_session_goto_url(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::CallArgs args = CallArgsFromVp(argc, rval);
	JS::RootedObject this_o(ctx, &args.thisv().toObject());

	struct delayed_open *deo;
	struct uri *uri;
	JSString *jsstr;
	char *url;
	struct session *ses;

	if (argc != 1) return false;

	if (!JS_InstanceOf(ctx, this_o, (JSClass *) &session_class, NULL))
		return false;

	ses = JS_GetInstancePrivate(ctx, this_o,
	                            (JSClass *) &session_class, NULL);
	if (!ses) return false; /* detached */

	jsstr = args[0].toString();
	if (!jsstr) return false;

	url = JS_EncodeString(ctx, jsstr);
	if (!url) return false;

	uri = get_uri(url, 0);
	if (!uri) return false;

	deo = mem_calloc(1, sizeof(*deo));
	if (!deo) {
		done_uri(uri);
		return false;
	}

	deo->ses = ses;
	deo->uri = get_uri_reference(uri);
	/* deo->target = NULL; */
	register_bottom_half(delayed_goto_uri_frame, deo);

	args.rval().setUndefined();

	done_uri(uri);

	return true;
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
