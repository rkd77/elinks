/* The SpiderMonkey console object implementation. */

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
#include "config/home.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/console.h"
#include "intl/libintl.h"
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

static bool console_error(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool console_log(JSContext *ctx, unsigned int argc, JS::Value *vp);

const spidermonkeyFunctionSpec console_funcs[] = {
	{ "log",		console_log,	 	1 },
	{ "error",		console_error,	 	1 },
	{ NULL }
};

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
