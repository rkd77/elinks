/* The MuJS Performance implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/ecmascript.h"
#include "js/mujs.h"
#include "js/mujs/performance.h"
#include "osdep/osdep.h"

static void
mjs_performance_get_property_timeOrigin(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	js_pushnumber(J, interpreter->time_origin);
}

static void
mjs_performance_now(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	js_pushnumber(J, get_time() - interpreter->time_origin);
}

void
mjs_push_performance(js_State *J)
{
	ELOG

	js_newobject(J);
	{
		addmethod(J, "performance.now", mjs_performance_now, 0);
		addproperty(J, "timeOrigin", mjs_performance_get_property_timeOrigin, NULL);
	}
}
