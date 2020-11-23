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

bool
PrintError(JSContext* cx, FILE* file, JS::ConstUTF8CharsZ toStringResult,
               JSErrorReport* report, bool reportWarnings)
{
    MOZ_ASSERT(report);

    /* Conditionally ignore reported warnings. */
    if (JSREPORT_IS_WARNING(report->flags) && !reportWarnings)
        return false;

    char* prefix = nullptr;
    if (report->filename)
        prefix = JS_smprintf("%s:", report->filename);
    if (report->lineno) {
        char* tmp = prefix;
        prefix = JS_smprintf("%s%u:%u ", tmp ? tmp : "", report->lineno, report->column);
        JS_free(cx, tmp);
    }
    if (JSREPORT_IS_WARNING(report->flags)) {
        char* tmp = prefix;
        prefix = JS_smprintf("%s%swarning: ",
                             tmp ? tmp : "",
                             JSREPORT_IS_STRICT(report->flags) ? "strict " : "");
        JS_free(cx, tmp);
    }

    const char* message = toStringResult ? toStringResult.c_str() : report->message().c_str();

    /* embedded newlines -- argh! */
    const char* ctmp;
    while ((ctmp = strchr(message, '\n')) != 0) {
        ctmp++;
        if (prefix)
            fputs(prefix, file);
        fwrite(message, 1, ctmp - message, file);
        message = ctmp;
    }

    /* If there were no filename or lineno, the prefix might be empty */
    if (prefix)
        fputs(prefix, file);
    fputs(message, file);

    if (const char16_t* linebuf = report->linebuf()) {
        size_t n = report->linebufLength();

        fputs(":\n", file);
        if (prefix)
            fputs(prefix, file);

        for (size_t i = 0; i < n; i++)
            fputc(static_cast<char>(linebuf[i]), file);

        // linebuf usually ends with a newline. If not, add one here.
        if (n == 0 || linebuf[n-1] != '\n')
            fputc('\n', file);

        if (prefix)
            fputs(prefix, file);

        n = report->tokenOffset();
        for (size_t i = 0, j = 0; i < n; i++) {
            if (linebuf[i] == '\t') {
                for (size_t k = (j + 8) & ~7; j < k; j++)
                    fputc('.', file);
                continue;
            }
            fputc('.', file);
            j++;
        }
        fputc('^', file);
    }
    fputc('\n', file);
    fflush(file);
    JS_free(cx, prefix);
    return true;
}



static void
error_reporter(JSContext *ctx, JSErrorReport *report)
{
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);
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
	add_to_string(&msg, report->message().c_str());

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

	static int initialized = 0;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;

	ctx = main_ctx;

	if (!ctx) {
		return nullptr;
	}

	interpreter->backend_data = ctx;
	interpreter->ar = new JSAutoRequest(ctx);
	//JSAutoRequest ar(ctx);

//	JS_SetContextPrivate(ctx, interpreter);

	//JS_SetOptions(main_ctx, JSOPTION_VAROBJFIX | JS_METHODJIT);
	JS::SetWarningReporter(ctx, error_reporter);
	JS_AddInterruptCallback(ctx, heartbeat_callback);
	JS::CompartmentOptions options;

	JS::RootedObject window_obj(ctx, JS_NewGlobalObject(ctx, &window_class, NULL, JS::FireOnNewGlobalHook, options));

	if (window_obj) {
		interpreter->ac = window_obj;
		interpreter->ac2 = new JSAutoCompartment(ctx, window_obj);
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
	//JS_SetPrivate(window_obj, interpreter); /* to @window_class */

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
	JS_SetCompartmentPrivate(js::GetContextCompartment(ctx), interpreter);

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
	if (interpreter->ac) {
		//delete (JSAutoCompartment *)interpreter->ac;
	}
	if (interpreter->ar) {
		delete (JSAutoRequest *)interpreter->ar;
	}
//	JS_DestroyContext(ctx);
	interpreter->backend_data = NULL;
	interpreter->ac = nullptr;
	interpreter->ar = nullptr;
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
	JS_BeginRequest(ctx);
	JSCompartment *comp = JS_EnterCompartment(ctx, interpreter->ac);

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;

	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS::RootedValue r_val(ctx, rval);
	JS::CompileOptions options(ctx);

	JS::Evaluate(ctx, options, code->source, code->length, &r_val);
	done_heartbeat(interpreter->heartbeat);
	JS_LeaveCompartment(ctx, comp);
	JS_EndRequest(ctx);
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
	ctx = interpreter->backend_data;
	JS_BeginRequest(ctx);
	JSCompartment *comp = JS_EnterCompartment(ctx, interpreter->ac);

	interpreter->heartbeat = add_heartbeat(interpreter);
	interpreter->ret = ret;

	JS::RootedValue r_val(ctx, rval);
	JS::RootedObject cg(ctx, JS::CurrentGlobalOrNull(ctx));
	JS_CallFunctionValue(ctx, cg, fun, JS::HandleValueArray::empty(), &r_val);
	done_heartbeat(interpreter->heartbeat);
	JS_LeaveCompartment(ctx, comp);
	JS_EndRequest(ctx);
}


unsigned char *
spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	bool ret;
	JSContext *ctx;
	JS::Value rval;
	unsigned char *result = NULL;

	assert(interpreter);
	if (!js_module_init_ok) return NULL;
	ctx = interpreter->backend_data;
	interpreter->ret = NULL;
	interpreter->heartbeat = add_heartbeat(interpreter);

	JS_BeginRequest(ctx);
	JSCompartment *comp = JS_EnterCompartment(ctx, interpreter->ac);

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
		result = NULL;
	}
	else if (r_rval.isNullOrUndefined()) {
		/* Undefined value. */
		result = NULL;
	} else {
		result = stracpy(JS_EncodeString(ctx, r_rval.toString()));
	}
	JS_LeaveCompartment(ctx, comp);
	JS_EndRequest(ctx);
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
	ctx = interpreter->backend_data;
	interpreter->ret = NULL;

	JSCompartment *comp = JS_EnterCompartment(ctx, interpreter->ac);
	JS_BeginRequest(ctx);

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

	JS_LeaveCompartment(ctx, comp);
	JS_EndRequest(ctx);

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
