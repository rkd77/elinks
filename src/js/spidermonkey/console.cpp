/* The SpiderMonkey console object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/spidermonkey/util.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/home.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "js/ecmascript.h"
#include "js/spidermonkey/console.h"
#include "intl/libintl.h"
#include "main/main.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"

#include <time.h>
#include "document/renderer.h"
#include "document/refresh.h"
#include "terminal/screen.h"

#define DEBUG 0

JSClassOps console_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

/* Each @console_class object must have a @window_class parent.  */
const JSClass console_class = {
	"console",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&console_ops
};

static bool console_assert(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool console_error(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool console_exit(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool console_log(JSContext *ctx, unsigned int argc, JS::Value *vp);

const spidermonkeyFunctionSpec console_funcs[] = {
	{ "assert",		console_assert,		2 },
	{ "log",		console_log,	 	1 },
	{ "error",		console_error,	 	1 },
	{ "exit",		console_exit,	 	0 },
	{ NULL }
};

static int assertions;
static int failed_assertions;

static bool
console_assert(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setUndefined();

	if (argc < 1 || !get_opt_bool("ecmascript.enable_console_log", NULL)) {
		return true;
	}
	assertions++;
	bool res = jsval_to_boolean(ctx, args[0]);

	if (res) {
		return true;
	}
	failed_assertions++;
	FILE *log = fopen(console_error_filename, "a");

	if (!log) {
		return true;
	}
	fprintf(log, "Assertion failed:");

	for (int i = 1; i < argc; i++) {
		char *val = jsval_to_string(ctx, args[i]);

		if (val) {
			fprintf(log, " %s", val);
			mem_free(val);
		}
	}
	fprintf(log, "\n");
	fclose(log);

	return true;
}

static bool
console_log_common(JSContext *ctx, unsigned int argc, JS::Value *vp, const char *log_filename)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);

	if (argc != 1 || !log_filename)
	{
		args.rval().setBoolean(false);
		return(true);
	}

	if (get_opt_bool("ecmascript.enable_console_log", NULL))
	{
		char *key = jsval_to_string(ctx, args[0]);

		FILE *f = fopen(log_filename, "a");

		if (f)
		{
			fprintf(f, "%s\n", key);
			fclose(f);
		}
		mem_free_if(key);
	}

	args.rval().setBoolean(true);
	return(true);
}

static bool
console_log(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return console_log_common(ctx, argc, vp, console_log_filename);
}

static bool
console_error(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return console_log_common(ctx, argc, vp, console_error_filename);
}

static bool
console_exit(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setUndefined();

	if (!program.testjs) {
		return true;
	}
	fprintf(stderr, "Assertions: %d, failed assertions: %d\n", assertions, failed_assertions);
	program.retval = failed_assertions ? RET_ERROR : RET_OK;
	program.terminate = 1;
	return true;
}
