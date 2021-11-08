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

	JSValue global_obj = JS_GetGlobalObject(ctx);
	js_window_init(ctx, global_obj);
	js_screen_init(ctx, global_obj);
	js_unibar_init(ctx, global_obj);
	js_navigator_init(ctx, global_obj);
	js_history_init(ctx, global_obj);
	js_location_init(ctx, global_obj);
	js_console_init(ctx, global_obj);
	js_localstorage_init(ctx, global_obj);
	interpreter->document_obj = js_document_init(ctx, global_obj);

	JS_FreeValue(ctx, global_obj);

	return ctx;
#if 0




	if (window_obj) {
		interpreter->ac = window_obj;
		interpreter->ac2 = new JSAutoRealm(ctx, window_obj);
	} else {
		goto release_and_fail;
	}

	if (!JS::InitRealmStandardClasses(ctx)) {
		goto release_and_fail;
	}

	if (!JS_DefineProperties(ctx, window_obj, window_props)) {
		goto release_and_fail;
	}

	if (!spidermonkey_DefineFunctions(ctx, window_obj, window_funcs)) {
		goto release_and_fail;
	}
	//JS_SetPrivate(window_obj, interpreter); /* to @window_class */

	document_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					      &document_class, NULL, 0,
					      document_props,
					      document_funcs,
					      NULL, NULL);
	if (!document_obj) {
		goto release_and_fail;
	}

	interpreter->document_obj = document_obj;

/*
	forms_obj = spidermonkey_InitClass(ctx, document_obj, NULL,
					   &forms_class, NULL, 0,
					   forms_props,
					   forms_funcs,
					   NULL, NULL);
	if (!forms_obj) {
		goto release_and_fail;
	}
*/

	history_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					     &history_class, NULL, 0,
					     (JSPropertySpec *) NULL,
					     history_funcs,
					     NULL, NULL);
	if (!history_obj) {
		goto release_and_fail;
	}

	location_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					      &location_class, NULL, 0,
					      location_props,
					      location_funcs,
					      NULL, NULL);
	if (!location_obj) {
		goto release_and_fail;
	}

	interpreter->location_obj = location_obj;

	screen_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					      &screen_class, NULL, 0,
					      screen_props,
					      NULL,
					      NULL, NULL);

	if (!screen_obj) {
		goto release_and_fail;
	}

	menubar_obj = JS_InitClass(ctx, window_obj, nullptr,
				   &menubar_class, NULL, 0,
				   unibar_props, NULL,
				   NULL, NULL);
	if (!menubar_obj) {
		goto release_and_fail;
	}
	JS_SetPrivate(menubar_obj, "t"); /* to @menubar_class */

	statusbar_obj = JS_InitClass(ctx, window_obj, nullptr,
				     &statusbar_class, NULL, 0,
				     unibar_props, NULL,
				     NULL, NULL);
	if (!statusbar_obj) {
		goto release_and_fail;
	}
	JS_SetPrivate(statusbar_obj, "s"); /* to @statusbar_class */

	navigator_obj = JS_InitClass(ctx, window_obj, nullptr,
				     &navigator_class, NULL, 0,
				     navigator_props, NULL,
				     NULL, NULL);
	if (!navigator_obj) {
		goto release_and_fail;
	}

	console_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					      &console_class, NULL, 0,
					      nullptr,
					      console_funcs,
					      NULL, NULL);
	if (!console_obj) {
		goto release_and_fail;
	}

	localstorage_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					      &localstorage_class, NULL, 0,
					      nullptr,
					      localstorage_funcs,
					      NULL, NULL);
	if (!localstorage_obj) {
		goto release_and_fail;
	}

	JS::SetRealmPrivate(js::GetContextRealm(ctx), interpreter);

	return ctx;

release_and_fail:
	spidermonkey_put_interpreter(interpreter);
	return NULL;

#endif
}

void
quickjs_put_interpreter(struct ecmascript_interpreter *interpreter)
{
#if 0
	JSContext *ctx;

	assert(interpreter);
	if (!js_module_init_ok) return;

	ctx = interpreter->backend_data;
	if (interpreter->ac2) {
		delete (JSAutoRealm *)interpreter->ac2;
	}
//	JS_DestroyContext(ctx);
	interpreter->backend_data = NULL;
	interpreter->ac = nullptr;
	interpreter->ac2 = nullptr;
#endif
}

#if 0
void
spidermonkey_check_for_exception(JSContext *ctx) {
	if (JS_IsExceptionPending(ctx))
	{
		JS::RootedValue exception(ctx);
	         if(JS_GetPendingException(ctx,&exception) && exception.isObject()) {
			JS::AutoSaveExceptionState savedExc(ctx);
			JS::Rooted<JSObject*> exceptionObject(ctx, &exception.toObject());
			JSErrorReport *report = JS_ErrorFromException(ctx,exceptionObject);
			if(report) {
				if (report->lineno>0) {
					/* Somehow the reporter alway reports first error
					 * Undefined and with line 0. Let's filter this. */
					/* Optional printing javascript error to file */
					//FILE *f = fopen("js.err","a");
					//PrintError(ctx, f, report->message(), report, true);
					/* Send the error to the tui */
					error_reporter(ctx, report);
					//DBG("file: %s",report->filename);
					//DBG("file: %s",report->message());
					//DBG("file: %d",(int) report->lineno);
				}
			}
			//JS_ClearPendingException(ctx);
		}
		/* This absorbs all following exceptions
		 * probably not the 100% correct solution
		 * to the javascript error handling but
		 * at least there isn't too much click-bait
		 * on each site with javascript enabled */
		JS_ClearPendingException(ctx);
	}
}
#endif

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
}

#if 0
void
quickjs_call_function(struct ecmascript_interpreter *interpreter,
                  JS::HandleValue fun, struct string *ret)
{
#if 0
	JSContext *ctx;
	JS::Value rval;

	assert(interpreter);
	if (!js_module_init_ok) {
		return;
	}
	ctx = interpreter->backend_data;
	JS::Realm *comp = JS::EnterRealm(ctx, interpreter->ac);

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;

	JS::RootedValue r_val(ctx, rval);
	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS_CallFunctionValue(ctx, cg, fun, JS::HandleValueArray::empty(), &r_val);
	done_heartbeat(interpreter->heartbeat);
	JS::LeaveRealm(ctx, comp);
#endif
}
#endif

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
#if 0
	JSContext *ctx;
	JS::Value rval;
	int ret;
	int result = 0;

	assert(interpreter);
	if (!js_module_init_ok) return 0;
	ctx = interpreter->backend_data;
	interpreter->ret = NULL;

	JS::Realm *comp = JS::EnterRealm(ctx, interpreter->ac);

	JS::CompileOptions options(ctx);
	JS::RootedObjectVector ag(ctx);

	JS::SourceText<mozilla::Utf8Unit> srcBuf;
	if (!srcBuf.init(ctx, code->source, code->length, JS::SourceOwnership::Borrowed)) {
		return -1;
	}

	JSFunction *funs = JS::CompileFunction(ctx, ag, options, "aaa", 0, nullptr, srcBuf);
	if (!funs) {
		return -1;
	};

	interpreter->heartbeat = add_heartbeat(interpreter);
	JS::RootedValue r_val(ctx, rval);
	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS::RootedFunction fun(ctx, funs);
	ret = JS_CallFunction(ctx, cg, fun, JS::HandleValueArray::empty(), &r_val);
	done_heartbeat(interpreter->heartbeat);

	if (ret == 2) { /* onClick="history.back()" */
		result = 0;
	}
	else if (ret == false) {
		result = -1;
	}
	else if (r_val.isUndefined()) {
		/* Undefined value. */
		result = -1;
	} else {
		result = r_val.toBoolean();
	}

	JS::LeaveRealm(ctx, comp);

	return result;
#endif
	return 0;
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
