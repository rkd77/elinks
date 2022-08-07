/* The mujs ECMAScript backend. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/xml/renderer.h"
#include "document/xml/renderer2.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/renderer.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/console.h"
#include "ecmascript/mujs/history.h"
#include "ecmascript/mujs/localstorage.h"
#include "ecmascript/mujs/navigator.h"
#include "ecmascript/mujs/screen.h"
#include "ecmascript/mujs/unibar.h"
#include "ecmascript/mujs/window.h"
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
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

#include <libxml++/libxml++.h>

/*** Global methods */


static void
mujs_init(struct module *xxx)
{
	//js_module_init_ok = spidermonkey_runtime_addref();
}

static void
mujs_done(struct module *xxx)
{
//	if (js_module_init_ok)
//		spidermonkey_runtime_release();
}

void *
mujs_get_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);

	js_State *J = js_newstate(NULL, NULL, JS_STRICT);

	if (!J) {
		return NULL;
	}
	interpreter->backend_data = J;
	js_setcontext(J, interpreter);
	mjs_window_init(J);
	mjs_screen_init(J);
	mjs_unibar_init(J);
	mjs_navigator_init(J);
	mjs_history_init(J);
	mjs_console_init(J);
	mjs_localstorage_init(J);

	return J;
#if 0

	JSContext *ctx;
//	JSObject *console_obj, *document_obj, /* *forms_obj,*/ *history_obj, *location_obj,
//	         *statusbar_obj, *menubar_obj, *navigator_obj, *localstorage_obj, *screen_obj;

	assert(interpreter);
//	if (!js_module_init_ok) return NULL;

	JSRuntime *rt = JS_NewRuntime();
	if (!rt) {
		return nullptr;
	}

	JS_SetMemoryLimit(rt, 64 * 1024 * 1024);
	JS_SetGCThreshold(rt, 16 * 1024 * 1024);

	ctx = JS_NewContext(rt);

	if (!ctx) {
		JS_FreeRuntime(rt);
		return nullptr;
	}

	interpreter->backend_data = ctx;
	JS_SetContextOpaque(ctx, interpreter);

//	JS::SetWarningReporter(ctx, error_reporter);

	JS_SetInterruptHandler(rt, js_heartbeat_callback, interpreter);
//	JS::RealmOptions options;

//	JS::RootedObject window_obj(ctx, JS_NewGlobalObject(ctx, &window_class, NULL, JS::FireOnNewGlobalHook, options));

	js_window_init(ctx);
	js_screen_init(ctx);
	js_unibar_init(ctx);
	js_navigator_init(ctx);
	js_history_init(ctx);
	js_console_init(ctx);
	js_localstorage_init(ctx);
	js_element_init(ctx);

	interpreter->document_obj = js_document_init(ctx);
	interpreter->location_obj = js_location_init(ctx);

	return ctx;
#endif
}

void
mujs_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	js_State *J = (js_State *)interpreter->backend_data;

	js_freestate(J);
	interpreter->backend_data = NULL;
#if 0
	JSContext *ctx;

	assert(interpreter);

	ctx = (JSContext *)interpreter->backend_data;
	JS_FreeContext(ctx);
	interpreter->backend_data = nullptr;
	interpreter->ac = nullptr;
	interpreter->ac2 = nullptr;
#endif
}

#if 0
static void
error_reporter(struct ecmascript_interpreter *interpreter, JSContext *ctx)
{
	struct session *ses = interpreter->vs->doc_view->session;
	struct terminal *term;
	struct string msg;
	struct string f;

	assert(interpreter && interpreter->vs && interpreter->vs->doc_view
	       && ses && ses->tab);
	if_assert_failed return;

	term = ses->tab->term;

#ifdef CONFIG_LEDS
	set_led_value(ses->status.ecmascript_led, 'J');
#endif

	if (!get_opt_bool("ecmascript.error_reporting", ses)) {
		return;
	}

	if (init_string(&f)) {
		js_dump_error(ctx, &f);

		if (!init_string(&msg)) {
			done_string(&f);
			return;
		} else {
			add_to_string(&msg,
			_("A script embedded in the current document raised the following:\n", term));
			add_string_to_string(&msg, &f);
			done_string(&f);
			info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER, msg.source);
		}
	}
}
#endif

void
mujs_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code, struct string *ret)
{
	assert(interpreter);

	js_State *J = (js_State *)interpreter->backend_data;
	interpreter->ret = ret;
	js_dostring(J, code->source);
#if 0
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;
	JSValue r = JS_Eval(ctx, code->source, code->length, "", 0);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsException(r)) {
		error_reporter(interpreter, ctx);
	}
#endif
}

void
mujs_call_function(struct ecmascript_interpreter *interpreter,
                  void *fun, struct string *ret)
{
#if 0
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;
	JSValue r = JS_Call(ctx, fun, JS_GetGlobalObject(ctx), 0, nullptr);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsException(r)) {
		error_reporter(interpreter, ctx);
	}
#endif
}

char *
mujs_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	return nullptr;
#if 0
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = nullptr;
	JSValue r = JS_Eval(ctx, code->source, code->length, "", 0);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsNull(r)) {
		return nullptr;
	}

	if (JS_IsException(r)) {
		error_reporter(interpreter, ctx);
	}

	const char *str;
	char *string;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, r);

	if (!str) {
		return nullptr;
	}
	string = stracpy(str);
	JS_FreeCString(ctx, str);

	return string;
#endif
}

int
mujs_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	return -1;
#if 0

	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = nullptr;
	JSValue r = JS_Eval(ctx, code->source, code->length, "", 0);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsNull(r)) {
		return -1;
	}

	if (JS_IsUndefined(r)) {
		return -1;
	}

	int ret = -1;

	JS_ToInt32(ctx, &ret, r);

	return ret;
#endif
}

struct module mujs_module = struct_module(
	/* name: */		N_("mujs"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		mujs_init,
	/* done: */		mujs_done
);
