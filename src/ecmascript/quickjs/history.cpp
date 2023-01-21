/* The QuickJS history object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

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
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/history.h"
#include "ecmascript/quickjs/window.h"
#include "intl/libintl.h"
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

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_history_class_id;

/* @history_funcs{"back"} */
static JSValue
js_history_back(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;

	go_back(ses);

/* history_back() must return 0 for onClick to cause displaying previous page
 * and return non zero for <a href="javascript:history.back()"> to prevent
 * "calculating" new link. Returned value 2 is changed to 0 in function
 * spidermonkey_eval_boolback */
	return JS_NULL;
}

/* @history_funcs{"forward"} */
static JSValue
js_history_forward(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;

	go_unback(ses);

	return JS_NULL;
}

/* @history_funcs{"go"} */
static JSValue
js_history_go(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	assert(interpreter);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct session *ses = doc_view->session;
	struct location *loc;

	if (argc != 1) {
		return JS_UNDEFINED;
	}

	int index;
	if (JS_ToInt32(ctx, &index, argv[0])) {
		return JS_UNDEFINED;
	}

	for (loc = cur_loc(ses);
	     loc != (struct location *) &ses->history.history;
	     loc = index > 0 ? loc->next : loc->prev) {
		if (!index) {
			go_history(ses, loc);
			break;
		}

		index += index > 0 ? -1 : 1;
	}

	return JS_NULL;
}

static JSValue
js_history_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[history object]");
}

static const JSCFunctionListEntry js_history_funcs[] = {
	JS_CFUNC_DEF("back", 0, js_history_back ),
	JS_CFUNC_DEF("forward", 0, js_history_forward ),
	JS_CFUNC_DEF("go", 1, js_history_go ),
	JS_CFUNC_DEF("toString", 0, js_history_toString)
};

static JSClassDef js_history_class = {
	"history",
};

static JSValue
js_history_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	REF_JS(new_target);

	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	REF_JS(proto);

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_history_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	RETURN_JS(obj);

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_history_init(JSContext *ctx)
{
	JSValue history_proto, history_class;

	/* create the history class */
	JS_NewClassID(&js_history_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_history_class_id, &js_history_class);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	REF_JS(global_obj);

	history_proto = JS_NewObject(ctx);
	REF_JS(history_proto);

	JS_SetPropertyFunctionList(ctx, history_proto, js_history_funcs, countof(js_history_funcs));

	history_class = JS_NewCFunction2(ctx, js_history_ctor, "history", 0, JS_CFUNC_constructor, 0);
	REF_JS(history_class);

	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, history_class, history_proto);
	JS_SetClassProto(ctx, js_history_class_id, history_proto);

	JS_SetPropertyStr(ctx, global_obj, "history", JS_DupValue(ctx, history_proto));

	JS_FreeValue(ctx, global_obj);

	return 0;
}
