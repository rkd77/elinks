/* ECMAScript browser scripting module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/home.h"
#include "ecmascript/spidermonkey/util.h"
#include "main/module.h"
#include "scripting/scripting.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/smjs.h"
#include "util/string.h"


#define SMJS_HOOKS_FILENAME "hooks.js"

JSContext *smjs_ctx;
JSObject *smjs_elinks_object;
struct session *smjs_ses;


void
alert_smjs_error(unsigned char *msg)
{
	report_scripting_error(&smjs_scripting_module,
	                       smjs_ses, msg);
}

static void
error_reporter(JSContext *ctx, const char *message, JSErrorReport *report)
{
	unsigned char *strict, *exception, *warning, *error;
	struct string msg;

	if (!init_string(&msg)) goto reported;

	strict	  = JSREPORT_IS_STRICT(report->flags) ? " strict" : "";
	exception = JSREPORT_IS_EXCEPTION(report->flags) ? " exception" : "";
	warning   = JSREPORT_IS_WARNING(report->flags) ? " warning" : "";
	error	  = !report->flags ? " error" : "";

	add_format_to_string(&msg, "A client script raised the following%s%s%s%s",
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

	alert_smjs_error(msg.source);
	done_string(&msg);

reported:
	JS_ClearPendingException(ctx);
}

static JSRuntime *smjs_rt;

void
smjs_load_hooks(void)
{
	jsval rval;
	struct string script;
	unsigned char *path;

	assert(smjs_ctx);

	if (!init_string(&script)) return;

	if (elinks_home) {
		path = straconcat(elinks_home, SMJS_HOOKS_FILENAME, NULL);
	} else {
		path = stracpy(CONFDIR "/" SMJS_HOOKS_FILENAME);
	}

	if (add_file_to_string(&script, path)
	     && JS_FALSE == JS_EvaluateScript(smjs_ctx,
				JS_GetGlobalObject(smjs_ctx),
				script.source, script.length, path, 1, &rval))
		alert_smjs_error("error loading default script file");

	mem_free(path);
	done_string(&script);
}

void
init_smjs(struct module *module)
{
	const JSClass global_class = {
		"global", 0,
		JS_PropertyStub, JS_PropertyStub,
		JS_PropertyStub, JS_PropertyStub,
		JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
	};
	JSObject *global_object;

	smjs_rt = JS_NewRuntime(1L * 1024L * 1024L);
	if (!smjs_rt) return;

	smjs_ctx = JS_NewContext(smjs_rt, 8192);
	if (!smjs_ctx) return;

	JS_SetErrorReporter(smjs_ctx, error_reporter);

	global_object = JS_NewObject(smjs_ctx, (JSClass *) &global_class,
	                             NULL, NULL);
	if (!global_object) return;

	JS_InitStandardClasses(smjs_ctx, global_object);

	smjs_elinks_object = smjs_get_elinks_object(global_object);

	smjs_load_hooks();
}

void
cleanup_smjs(struct module *module)
{
	if (!smjs_ctx) return;

	JS_DestroyContext(smjs_ctx);
	JS_DestroyRuntime(smjs_rt);
}
