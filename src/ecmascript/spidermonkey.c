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

	if (report->linebuf && report->tokenptr) {
		int pos = report->tokenptr - report->linebuf;

		add_format_to_string(&msg, "\n\n%s\n.%*s^%*s.",
			       report->linebuf,
			       pos - 2, " ",
			       strlen(report->linebuf) - pos - 1, " ");
	}

	info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER,
		 msg.source);

reported:
	/* Im clu'les. --pasky */
	JS_ClearPendingException(ctx);
}

#if !defined(CONFIG_ECMASCRIPT_SMJS_HEARTBEAT) && defined(HAVE_JS_SETBRANCHCALLBACK)
static JSBool
safeguard(JSContext *ctx, JSScript *script)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct session *ses = interpreter->vs->doc_view->session;
	int max_exec_time = get_opt_int("ecmascript.max_exec_time", ses);

	if (time(NULL) - interpreter->exec_start > max_exec_time) {
		struct terminal *term = ses->tab->term;

		/* A killer script! Alert! */
		ecmascript_timeout_dialog(term, max_exec_time);
		return JS_FALSE;
	}
	return JS_TRUE;
}

static void
setup_safeguard(struct ecmascript_interpreter *interpreter,
                JSContext *ctx)
{
	interpreter->exec_start = time(NULL);
	JS_SetBranchCallback(ctx, safeguard);
}
#endif


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
	JSObject *window_obj, *document_obj, *forms_obj, *history_obj, *location_obj,
	         *statusbar_obj, *menubar_obj, *navigator_obj;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;

	ctx = JS_NewContext(spidermonkey_runtime,
			    8192 /* Stack allocation chunk size */);
	if (!ctx)
		return NULL;
	interpreter->backend_data = ctx;
	JS_SetContextPrivate(ctx, interpreter);
	JS_SetOptions(ctx, JSOPTION_VAROBJFIX | JSOPTION_JIT | JSOPTION_METHODJIT);
	JS_SetVersion(ctx, JSVERSION_LATEST);
	JS_SetErrorReporter(ctx, error_reporter);
#if defined(CONFIG_ECMASCRIPT_SMJS_HEARTBEAT)
	JS_SetOperationCallback(ctx, heartbeat_callback);
#endif

	window_obj = JS_NewCompartmentAndGlobalObject(ctx, (JSClass *) &window_class, NULL);
	if (!window_obj) goto release_and_fail;
	if (!JS_InitStandardClasses(ctx, window_obj)) goto release_and_fail;
	if (!JS_DefineProperties(ctx, window_obj, (JSPropertySpec *) window_props))
		goto release_and_fail;
	if (!spidermonkey_DefineFunctions(ctx, window_obj, window_funcs))
		goto release_and_fail;
	if (!JS_SetPrivate(ctx, window_obj, interpreter->vs)) /* to @window_class */
		goto release_and_fail;

	document_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					      (JSClass *) &document_class, NULL, 0,
					      (JSPropertySpec *) document_props,
					      document_funcs,
					      NULL, NULL);
	if (!document_obj) goto release_and_fail;

	forms_obj = spidermonkey_InitClass(ctx, document_obj, NULL,
					   (JSClass *) &forms_class, NULL, 0,
					   (JSPropertySpec *) forms_props,
					   forms_funcs,
					   NULL, NULL);
	if (!forms_obj) goto release_and_fail;

	history_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					     (JSClass *) &history_class, NULL, 0,
					     (JSPropertySpec *) NULL,
					     history_funcs,
					     NULL, NULL);
	if (!history_obj) goto release_and_fail;

	location_obj = spidermonkey_InitClass(ctx, window_obj, NULL,
					      (JSClass *) &location_class, NULL, 0,
					      (JSPropertySpec *) location_props,
					      location_funcs,
					      NULL, NULL);
	if (!location_obj) goto release_and_fail;

	menubar_obj = JS_InitClass(ctx, window_obj, NULL,
				   (JSClass *) &menubar_class, NULL, 0,
				   (JSPropertySpec *) unibar_props, NULL,
				   NULL, NULL);
	if (!menubar_obj) goto release_and_fail;
	if (!JS_SetPrivate(ctx, menubar_obj, "t")) /* to @menubar_class */
		goto release_and_fail;

	statusbar_obj = JS_InitClass(ctx, window_obj, NULL,
				     (JSClass *) &statusbar_class, NULL, 0,
				     (JSPropertySpec *) unibar_props, NULL,
				     NULL, NULL);
	if (!statusbar_obj) goto release_and_fail;
	if (!JS_SetPrivate(ctx, statusbar_obj, "s")) /* to @statusbar_class */
		goto release_and_fail;

	navigator_obj = JS_InitClass(ctx, window_obj, NULL,
				     (JSClass *) &navigator_class, NULL, 0,
				     (JSPropertySpec *) navigator_props, NULL,
				     NULL, NULL);
	if (!navigator_obj) goto release_and_fail;

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
	jsval rval;

	assert(interpreter);
	if (!js_module_init_ok) return;
	ctx = interpreter->backend_data;
#if defined(CONFIG_ECMASCRIPT_SMJS_HEARTBEAT)
	interpreter->heartbeat = add_heartbeat(interpreter);
#elif defined(HAVE_JS_SETBRANCHCALLBACK)
	setup_safeguard(interpreter, ctx);
#endif
	interpreter->ret = ret;
	JS_EvaluateScript(ctx, JS_GetGlobalObject(ctx),
	                  code->source, code->length, "", 0, &rval);
#if defined(CONFIG_ECMASCRIPT_SMJS_HEARTBEAT)
	done_heartbeat(interpreter->heartbeat);
#endif
}


unsigned char *
spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	JSBool ret;
	JSContext *ctx;
	jsval rval;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;
	ctx = interpreter->backend_data;
	interpreter->ret = NULL;
#if defined(CONFIG_ECMASCRIPT_SMJS_HEARTBEAT)
	interpreter->heartbeat = add_heartbeat(interpreter);
#elif defined(HAVE_JS_SETBRANCHCALLBACK)
	setup_safeguard(interpreter, ctx);
#endif
	ret = JS_EvaluateScript(ctx, JS_GetGlobalObject(ctx),
	                        code->source, code->length, "", 0, &rval);
#if defined(CONFIG_ECMASCRIPT_SMJS_HEARTBEAT)
	done_heartbeat(interpreter->heartbeat);
#endif
	if (ret == JS_FALSE) {
		return NULL;
	}
	if (JSVAL_IS_VOID(rval) || JSVAL_IS_NULL(rval)) {
		/* Undefined value. */
		return NULL;
	}

	return stracpy(jsval_to_string(ctx, &rval));
}


int
spidermonkey_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	JSContext *ctx;
	JSFunction *fun;
	jsval rval;
	int ret;

	assert(interpreter);
	if (!js_module_init_ok) return 0;
	ctx = interpreter->backend_data;
	interpreter->ret = NULL;
	fun = JS_CompileFunction(ctx, NULL, "", 0, NULL, code->source,
				 code->length, "", 0);
	if (!fun)
		return -1;

#if defined(CONFIG_ECMASCRIPT_SMJS_HEARTBEAT)
	interpreter->heartbeat = add_heartbeat(interpreter);
#elif defined(HAVE_JS_SETBRANCHCALLBACK)
	setup_safeguard(interpreter, ctx);
#endif
	ret = JS_CallFunction(ctx, NULL, fun, 0, NULL, &rval);
#if defined(CONFIG_ECMASCRIPT_SMJS_HEARTBEAT)
	done_heartbeat(interpreter->heartbeat);
#endif
	if (ret == 2) { /* onClick="history.back()" */
		return 0;
	}
	if (ret == JS_FALSE) {
		return -1;
	}
	if (JSVAL_IS_VOID(rval)) {
		/* Undefined value. */
		return -1;
	}

	return jsval_to_boolean(ctx, &rval);
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
