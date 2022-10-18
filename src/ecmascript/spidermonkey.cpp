/* The SpiderMonkey ECMAScript backend. */

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
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/console.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/history.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/localstorage.h"
#include "ecmascript/spidermonkey/navigator.h"
#include "ecmascript/spidermonkey/screen.h"
#include "ecmascript/spidermonkey/unibar.h"
#include "ecmascript/spidermonkey/window.h"
#include "ecmascript/spidermonkey/xhr.h"
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

#include <js/CompilationAndEvaluation.h>
#include <js/Printf.h>
#include <js/SourceText.h>
#include <js/Warnings.h>

#include <libxml++/libxml++.h>

/*** Global methods */


/* TODO? Are there any which need to be implemented? */

static int js_module_init_ok;

static void
error_reporter(JSContext *ctx, JSErrorReport *report)
{
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
		return;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
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
		JS::PrintError(f, report, true/*reportWarnings*/);
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

static char spidermonkey_version[32];

static void
spidermonkey_init(struct module *module)
{
	snprintf(spidermonkey_version, 31, "mozjs %s", JS_GetImplementationVersion());
	module->name = spidermonkey_version;

	js_module_init_ok = spidermonkey_runtime_addref();
}

static void
spidermonkey_done(struct module *xxx)
{
	if (js_module_init_ok)
		spidermonkey_runtime_release();
}


void *
spidermonkey_get_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSContext *ctx;
	JSObject *console_obj, *document_obj, /* *forms_obj,*/ *history_obj, *location_obj,
	         *statusbar_obj, *menubar_obj, *navigator_obj, *localstorage_obj, *screen_obj,
	         *xhr_obj;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;

	ctx = main_ctx;

	if (!ctx) {
		return nullptr;
	}

	interpreter->backend_data = ctx;

	// JS_SetContextPrivate(ctx, interpreter);

	JS::SetWarningReporter(ctx, error_reporter);

	JS_AddInterruptCallback(ctx, heartbeat_callback);
	JS::RealmOptions options;

	JS::RootedObject window_obj(ctx, JS_NewGlobalObject(ctx, &window_class, NULL, JS::FireOnNewGlobalHook, options));

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
	JS::SetReservedSlot(menubar_obj, 0, JS::PrivateValue((char *)"t")); /* to @menubar_class */

	statusbar_obj = JS_InitClass(ctx, window_obj, nullptr,
				     &statusbar_class, NULL, 0,
				     unibar_props, NULL,
				     NULL, NULL);
	if (!statusbar_obj) {
		goto release_and_fail;
	}
	JS::SetReservedSlot(statusbar_obj, 0, JS::PrivateValue((char *)"s")); /* to @statusbar_class */

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

	xhr_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					&xhr_class, xhr_constructor, 0,
					xhr_props,
					xhr_funcs,
					xhr_static_props, NULL);

	if (!xhr_obj) {
		goto release_and_fail;
	}

	JS::SetRealmPrivate(js::GetContextRealm(ctx), interpreter);

	return ctx;

release_and_fail:
	spidermonkey_put_interpreter(interpreter);
	return NULL;
}

void
spidermonkey_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	if (!js_module_init_ok) return;

	if (interpreter->ac2) {
		delete (JSAutoRealm *)interpreter->ac2;
	}
//	JS_DestroyContext(ctx);
	interpreter->backend_data = NULL;
	interpreter->ac = nullptr;
	interpreter->ac2 = nullptr;
}

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
					//PrintError(f, report->message(), report, true);
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

void
spidermonkey_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code, struct string *ret)
{
	JSContext *ctx;
	JS::Value rval;

	assert(interpreter);
	if (!js_module_init_ok) {
		return;
	}
	ctx = (JSContext *)interpreter->backend_data;
	JS::Realm *comp = JS::EnterRealm(ctx, (JSObject *)interpreter->ac);

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;

	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS::RootedValue r_val(ctx, rval);
	JS::CompileOptions options(ctx);

	JS::SourceText<mozilla::Utf8Unit> srcBuf;
	if (!srcBuf.init(ctx, code->source, code->length, JS::SourceOwnership::Borrowed)) {
		return;
	}
	JS::Evaluate(ctx, options, srcBuf, &r_val);

	spidermonkey_check_for_exception(ctx);

	done_heartbeat(interpreter->heartbeat);
	JS::LeaveRealm(ctx, comp);
}

void
spidermonkey_call_function(struct ecmascript_interpreter *interpreter,
                  JS::HandleValue fun, struct string *ret)
{
	JSContext *ctx;
	JS::Value rval;

	assert(interpreter);
	if (!js_module_init_ok) {
		return;
	}
	ctx = (JSContext *)interpreter->backend_data;
	JS::Realm *comp = JS::EnterRealm(ctx, (JSObject *)interpreter->ac);

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;

	JS::RootedValue r_val(ctx, rval);
	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS_CallFunctionValue(ctx, cg, fun, JS::HandleValueArray::empty(), &r_val);
	done_heartbeat(interpreter->heartbeat);
	JS::LeaveRealm(ctx, comp);
}


char *
spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	bool ret;
	JSContext *ctx;
	JS::Value rval;
	char *result = NULL;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->ret = NULL;
	interpreter->heartbeat = add_heartbeat(interpreter);

	JS::Realm *comp = JS::EnterRealm(ctx, (JSObject *)interpreter->ac);

	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS::RootedValue r_rval(ctx, rval);
	JS::CompileOptions options(ctx);

//	options.setIntroductionType("js shell load")
//	.setUTF8(true)
//	.setCompileAndGo(true)
//	.setNoScriptRval(true);

	JS::SourceText<mozilla::Utf8Unit> srcBuf;
	if (!srcBuf.init(ctx, code->source, code->length, JS::SourceOwnership::Borrowed)) {
		return NULL;
	}
	ret = JS::Evaluate(ctx, options, srcBuf, &r_rval);
	done_heartbeat(interpreter->heartbeat);

	if (ret == false) {
		result = NULL;
	}
	else if (r_rval.isNullOrUndefined()) {
		/* Undefined value. */
		result = NULL;
	} else {
		result = jsval_to_string(ctx, r_rval);
	}
	JS::LeaveRealm(ctx, comp);

	return result;
}

int
spidermonkey_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	JSContext *ctx;
	JS::Value rval;
	int ret;
	int result = 0;

	assert(interpreter);
	if (!js_module_init_ok) return 0;
	ctx = (JSContext *)interpreter->backend_data;
	interpreter->ret = NULL;

	JS::Realm *comp = JS::EnterRealm(ctx, (JSObject *)interpreter->ac);

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
}

struct module spidermonkey_module = struct_module(
	/* name: */		N_("SpiderMonkey"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		spidermonkey_init,
	/* done: */		spidermonkey_done
);
