/* The MuJS console object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/ecmascript.h"
#include "js/mujs.h"
#include "js/mujs/console.h"
#include "main/main.h"

#define DEBUG 0

static int assertions;
static int failed_assertions;

static void
mjs_console_assert(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (!get_opt_bool("ecmascript.enable_console_log", NULL)) {
		js_pushundefined(J);
		return;
	}
	bool res = js_toboolean(J, 1);
	assertions++;

	if (res) {
		js_pushundefined(J);
		return;
	}
	FILE *log = fopen(console_error_filename, "a");
	failed_assertions++;

	if (!log) {
		js_pushundefined(J);
		return;
	}
	fprintf(log, "Assertion failed:");

	for (int i = 2;; i++) {
		if (js_isundefined(J, i)) {
			break;
		}
		const char *val = js_tostring(J, i);

		if (val) {
			fprintf(log, " %s", val);
		}
	}
	fprintf(log, "\n");
	fclose(log);
	js_pushundefined(J);
}

static void
mjs_console_log_common(js_State *J, const char *str, const char *log_filename)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (log_filename && get_opt_bool("ecmascript.enable_console_log", NULL) && str)
	{
		FILE *f = fopen(log_filename, "a");

		if (f)
		{
			fprintf(f, "%s\n", str);
			fclose(f);
		}
	}
	js_pushundefined(J);
}

static void
mjs_console_log(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	mjs_console_log_common(J, str, console_log_filename);
}

static void
mjs_console_error(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	mjs_console_log_common(J, str, console_error_filename);
}

static void
mjs_console_warn(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	mjs_console_log_common(J, str, console_warn_filename);
}


static void
mjs_console_exit(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (!program.testjs) {
		js_pushundefined(J);
		return;
	}
	fprintf(stderr, "Assertions: %d, failed assertions: %d\n", assertions, failed_assertions);
	program.retval = failed_assertions ? RET_ERROR : RET_OK;
	program.terminate = 1;
	js_pushundefined(J);
}

static void
mjs_console_toString(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[console object]");
}

int
mjs_console_init(js_State *J)
{
	ELOG
	js_newobject(J);
	{
		addmethod(J, "console.assert", mjs_console_assert, 2);
		addmethod(J, "console.error", mjs_console_error, 1);
		addmethod(J, "console.exit", mjs_console_exit, 0);
		addmethod(J, "console.log", mjs_console_log, 1);
		addmethod(J, "console.toString", mjs_console_toString, 0);
		addmethod(J, "console.warn", mjs_console_warn, 1);
	}
	js_defglobal(J, "console", JS_DONTENUM);

	return 0;
}
