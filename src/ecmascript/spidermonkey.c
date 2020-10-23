/* The SpiderMonkey ECMAScript backend. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include <jsprf.h>

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/location.h"
#include "ecmascript/spidermonkey/navigator.h"
#include "ecmascript/spidermonkey/unibar.h"
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
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

/*** Global methods */


/* TODO? Are there any which need to be implemented? */

static int js_module_init_ok;

static void
error_reporter(JSContext *ctx, const char *message, JSErrorReport *report)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct session *ses = interpreter->vs->doc_view->session;
	struct terminal *term;
	unsigned char *strict, *exception, *warning, *error;
	struct string msg;

	char *prefix = nullptr;

	assert(interpreter && interpreter->vs && interpreter->vs->doc_view
	       && ses && ses->tab);
	if_assert_failed goto reported;

	term = ses->tab->term;

#ifdef CONFIG_LEDS
	set_led_value(ses->status.ecmascript_led, 'J');
#endif

	if (!get_opt_bool("ecmascript.error_reporting", ses)
	    || !init_string(&msg))
		goto reported;

	strict	  = JSREPORT_IS_STRICT(report->flags) ? " strict" : "";
	exception = JSREPORT_IS_EXCEPTION(report->flags) ? " exception" : "";
	warning   = JSREPORT_IS_WARNING(report->flags) ? " warning" : "";
	error	  = !report->flags ? " error" : "";

	add_format_to_string(&msg, _("A script embedded in the current "
			"document raised the following%s%s%s%s", term),
			strict, exception, warning, error);

	add_to_string(&msg, ":\n\n");
	add_to_string(&msg, message);

	if (report->filename) {
		prefix = JS_smprintf("%s:", report->filename);
	}

	if (report->lineno) {
		char* tmp = prefix;
		prefix = JS_smprintf("%s%u:%u ", tmp ? tmp : "", report->lineno, report->column);
		JS_free(ctx, tmp);
	}

	if (prefix) {
		add_to_string(&msg, prefix);
	}
#if 0
	if (report->linebuf && report->tokenptr) {
		int pos = report->tokenptr - report->linebuf;

		add_format_to_string(&msg, "\n\n%s\n.%*s^%*s.",
			       report->linebuf,
			       pos - 2, " ",
			       strlen(report->linebuf) - pos - 1, " ");
	}
#endif

	info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER,
		 msg.source);

reported:
	/* Im clu'les. --pasky */
	JS_ClearPendingException(ctx);
}

static void
spidermonkey_init(struct module *xxx)
{
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
	JSObject *document_obj, *forms_obj, *history_obj, *location_obj,
	         *statusbar_obj, *menubar_obj, *navigator_obj;
	JSAutoCompartment *ac = NULL;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;

	ctx = JS_NewContext(spidermonkey_runtime,
			    8192 /* Stack allocation chunk size */);
	if (!ctx)
		return NULL;
	interpreter->backend_data = ctx;
	JSAutoRequest ar(ctx);
	JS_SetContextPrivate(ctx, interpreter);
	//JS_SetOptions(ctx, JSOPTION_VAROBJFIX | JS_METHODJIT);
	JS_SetErrorReporter(spidermonkey_runtime, error_reporter);
	JS_SetInterruptCallback(spidermonkey_runtime, heartbeat_callback);
	JS::RootedObject window_obj(ctx, JS_NewGlobalObject(ctx, &window_class, NULL, JS::DontFireOnNewGlobalHook));

	if (window_obj) {
		ac = new JSAutoCompartment(ctx, window_obj);
	} else {
		goto release_and_fail;
	}

	if (!JS_InitStandardClasses(ctx, window_obj)) {
		goto release_and_fail;
	}

	if (!JS_DefineProperties(ctx, window_obj, window_props)) {
		goto release_and_fail;
	}

	if (!spidermonkey_DefineFunctions(ctx, window_obj, window_funcs)) {
		goto release_and_fail;
	}
	JS_SetPrivate(window_obj, interpreter->vs); /* to @window_class */

	document_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					      &document_class, NULL, 0,
					      document_props,
					      document_funcs,
					      NULL, NULL);
	if (!document_obj) {
		goto release_and_fail;
	}

	forms_obj = spidermonkey_InitClass(ctx, document_obj, NULL,
					   &forms_class, NULL, 0,
					   forms_props,
					   forms_funcs,
					   NULL, NULL);
	if (!forms_obj) {
		goto release_and_fail;
	}
//	JS_SetPrivate(forms_obj, interpreter->vs);

	history_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					     &history_class, NULL, 0,
					     (JSPropertySpec *) NULL,
					     history_funcs,
					     NULL, NULL);
	if (!history_obj) {
		goto release_and_fail;
	}
//	JS_SetPrivate(history_obj, interpreter->vs);


	location_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					      &location_class, NULL, 0,
					      location_props,
					      location_funcs,
					      NULL, NULL);
	if (!location_obj) {
		goto release_and_fail;
	}
//	JS_SetPrivate(location_obj, interpreter->vs);


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
//	JS_SetPrivate(navigator_obj, interpreter->vs);

	return ctx;

release_and_fail:
	spidermonkey_put_interpreter(interpreter);
	return NULL;
}

void
spidermonkey_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSContext *ctx;

	assert(interpreter);
	if (!js_module_init_ok) return;
	ctx = interpreter->backend_data;
	JS_DestroyContext(ctx);
	interpreter->backend_data = NULL;
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
	ctx = interpreter->backend_data;

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;

	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS::RootedValue r_val(ctx, rval);
	JS::CompileOptions options(ctx);

	JS::Evaluate(ctx, options, code->source, code->length, &r_val);
	done_heartbeat(interpreter->heartbeat);
}


unsigned char *
spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	bool ret;
	JSContext *ctx;
	JS::Value rval;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;
	ctx = interpreter->backend_data;
	interpreter->ret = NULL;
	interpreter->heartbeat = add_heartbeat(interpreter);

	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS::RootedValue r_rval(ctx, rval);
	JS::CompileOptions options(ctx);

//	options.setIntroductionType("js shell load")
//	.setUTF8(true)
//	.setCompileAndGo(true)
//	.setNoScriptRval(true);

	ret = JS::Evaluate(ctx, options, code->source, code->length, &r_rval);
	done_heartbeat(interpreter->heartbeat);

	if (ret == false) {
		return NULL;
	}
	if (r_rval.isNullOrUndefined()) {
		/* Undefined value. */
		return NULL;
	}

	return stracpy(JS_EncodeString(ctx, r_rval.toString()));
}


int
spidermonkey_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	JSContext *ctx;
	JS::Value rval;
	int ret;

	assert(interpreter);
	if (!js_module_init_ok) return 0;
	ctx = interpreter->backend_data;
	interpreter->ret = NULL;

	JS::RootedFunction fun(ctx);

	JS::CompileOptions options(ctx);
	JS::AutoObjectVector ag(ctx);
	if (!JS::CompileFunction(ctx, ag, options, "aaa", 0, nullptr, code->source,
				 code->length, &fun)) {
		return -1;
	};

	interpreter->heartbeat = add_heartbeat(interpreter);
	JS::RootedValue r_val(ctx, rval);
	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	ret = JS_CallFunction(ctx, cg, fun, JS::HandleValueArray::empty(), &r_val);
	done_heartbeat(interpreter->heartbeat);

	if (ret == 2) { /* onClick="history.back()" */
		return 0;
	}
	if (ret == false) {
		return -1;
	}
	if (r_val.isUndefined()) {
		/* Undefined value. */
		return -1;
	}

	return r_val.toBoolean();
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
