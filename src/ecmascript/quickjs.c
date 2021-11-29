/* The Quickjs ECMAScript backend. */

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
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/console.h"
#include "ecmascript/quickjs/document.h"
#include "ecmascript/quickjs/element.h"
#include "ecmascript/quickjs/heartbeat.h"
#include "ecmascript/quickjs/history.h"
#include "ecmascript/quickjs/localstorage.h"
#include "ecmascript/quickjs/location.h"
#include "ecmascript/quickjs/navigator.h"
#include "ecmascript/quickjs/screen.h"
#include "ecmascript/quickjs/unibar.h"
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
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

#include <libxml++/libxml++.h>

/*** Global methods */


#if 0
/* TODO? Are there any which need to be implemented? */

static int js_module_init_ok;

static void
error_reporter(JSContext *ctx, JSErrorReport *report)
{
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
		return;
	}
	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);
	struct session *ses = interpreter->vs->doc_view->session;
	struct terminal *term;
	struct string msg;
	char *ptr;
	size_t size;
	FILE *f;

	assert(interpreter && interpreter->vs && interpreter->vs->doc_view
	       && ses && ses->tab);
	if_assert_failed goto reported;

	term = ses->tab->term;

#ifdef CONFIG_LEDS
	set_led_value(ses->status.ecmascript_led, 'J');
#endif

	if (!get_opt_bool("ecmascript.error_reporting", ses))
		goto reported;

	f = open_memstream(&ptr, &size);

	if (f) {
		JS::PrintError(ctx, f, report, true/*reportWarnings*/);
		fclose(f);

		if (!init_string(&msg)) {
			free(ptr);
		} else {
			add_to_string(&msg,
			_("A script embedded in the current document raised the following:\n", term));
			add_bytes_to_string(&msg, ptr, size);
			free(ptr);

			info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER, msg.source);
		}
	}
reported:
	JS_ClearPendingException(ctx);
}
#endif

static void
quickjs_init(struct module *xxx)
{
	//js_module_init_ok = spidermonkey_runtime_addref();
}

static void
quickjs_done(struct module *xxx)
{
//	if (js_module_init_ok)
//		spidermonkey_runtime_release();
}

void *
quickjs_get_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSContext *ctx;
//	JSObject *console_obj, *document_obj, /* *forms_obj,*/ *history_obj, *location_obj,
//	         *statusbar_obj, *menubar_obj, *navigator_obj, *localstorage_obj, *screen_obj;

	static int initialized = 0;

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
}

void
quickjs_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSContext *ctx;

	assert(interpreter);

	ctx = interpreter->backend_data;
	JS_FreeContext(ctx);
	interpreter->backend_data = nullptr;
	interpreter->ac = nullptr;
	interpreter->ac2 = nullptr;
}

static void
js_dump_obj(JSContext *ctx, FILE *f, JSValueConst val)
{
	const char *str;

	str = JS_ToCString(ctx, val);

	if (str) {
		fprintf(f, "%s\n", str);
		JS_FreeCString(ctx, str);
	} else {
		fprintf(f, "[exception]\n");
	}
}

static void
js_dump_error1(JSContext *ctx, FILE *f, JSValueConst exception_val)
{
	JSValue val;
	bool is_error;

	is_error = JS_IsError(ctx, exception_val);
	js_dump_obj(ctx, f, exception_val);

	if (is_error) {
		val = JS_GetPropertyStr(ctx, exception_val, "stack");

		if (!JS_IsUndefined(val)) {
			js_dump_obj(ctx, f, val);
		}
		JS_FreeValue(ctx, val);
	}
}

static void
js_dump_error(JSContext *ctx, FILE *f)
{
	JSValue exception_val;

	exception_val = JS_GetException(ctx);
	js_dump_error1(ctx, f, exception_val);
	JS_FreeValue(ctx, exception_val);
}

static void
error_reporter(struct ecmascript_interpreter *interpreter, JSContext *ctx)
{
	struct session *ses = interpreter->vs->doc_view->session;
	struct terminal *term;
	struct string msg;
	char *ptr;
	size_t size;
	FILE *f;

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

	f = open_memstream(&ptr, &size);

	if (f) {
		js_dump_error(ctx, f);
		fclose(f);

		if (!init_string(&msg)) {
			free(ptr);
		} else {
			add_to_string(&msg,
			_("A script embedded in the current document raised the following:\n", term));
			add_bytes_to_string(&msg, ptr, size);
			free(ptr);

			info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER, msg.source);
		}
	}
}

void
quickjs_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code, struct string *ret)
{
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = interpreter->backend_data;
	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;
	JSValue r = JS_Eval(ctx, code->source, code->length, "", 0);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsException(r)) {
		error_reporter(interpreter, ctx);
	}
}

void
quickjs_call_function(struct ecmascript_interpreter *interpreter,
                  JSValueConst fun, struct string *ret)
{
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = interpreter->backend_data;

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;
	JSValue r = JS_Call(ctx, fun, JS_GetGlobalObject(ctx), 0, nullptr);
	done_heartbeat(interpreter->heartbeat);

	if (JS_IsException(r)) {
		error_reporter(interpreter, ctx);
	}
}

char *
quickjs_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = interpreter->backend_data;
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

	const char *str, *string;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, r);

	if (!str) {
		return nullptr;
	}
	string = stracpy(str);
	JS_FreeCString(ctx, str);

	return string;
}

int
quickjs_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	JSContext *ctx;

	assert(interpreter);
//	if (!js_module_init_ok) {
//		return;
//	}
	ctx = interpreter->backend_data;
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
}

struct module quickjs_module = struct_module(
	/* name: */		N_("QuickJS"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		quickjs_init,
	/* done: */		quickjs_done
);
