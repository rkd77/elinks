/* The QuickJS console object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/home.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/console.h"
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

static void
mjs_console_log_common(js_State *J, const char *str, const char *log_filename)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	assert(interpreter);

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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	mjs_console_log_common(J, str, console_log_filename);
}

static void
mjs_console_error(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	mjs_console_log_common(J, str, console_error_filename);
}

static void
mjs_console_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[console object]");
}

int
mjs_console_init(js_State *J)
{
	js_newobject(J);
	{
		js_newcfunction(J, mjs_console_log, "console.log", 1);
		js_defproperty(J, -2, "log", JS_DONTENUM);

		js_newcfunction(J, mjs_console_error, "console.error", 1);
		js_defproperty(J, -2, "error", JS_DONTENUM);

		js_newcfunction(J, mjs_console_toString, "console.toString", 0);
		js_defproperty(J, -2, "toString", JS_DONTENUM);
	}
	js_defglobal(J, "console", JS_DONTENUM);

	return 0;
}
