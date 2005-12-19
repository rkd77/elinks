/* Ruby scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "ecmascript/spidermonkey/util.h"
#include "main/event.h"
#include "main/module.h"
#include "scripting/smjs/cache_object.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/hooks.h"
#include "session/session.h"


/* If elinks.<hook> is defined, call it with the given arguments,
 * store the return value in rval, and return JS_TRUE. Else return JS_FALSE. */
static JSBool
call_script_hook(unsigned char *hook, jsval argv[], int argc, jsval *rval)
{
	JSFunction *func;

	assert(smjs_ctx);
	assert(smjs_elinks_object);
	assert(rval);
	assert(argv);

	if (JS_FALSE == JS_GetProperty(smjs_ctx, smjs_elinks_object,
	                               hook, rval))
		return JS_FALSE;

	if (JSVAL_VOID == *rval)
		return JS_FALSE;

	func = JS_ValueToFunction(smjs_ctx, *rval);
	assert(func);

	return JS_CallFunction(smjs_ctx, smjs_elinks_object,
		               func, argc, argv, rval);
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);
	struct cache_entry *cached = va_arg(ap, struct cache_entry *);
	enum evhook_status ret = EVENT_HOOK_STATUS_NEXT;
	JSObject *cache_entry_object;
	jsval args[1], rval;

	evhook_use_params(ses && cached);

	if (!smjs_ctx || !cached->length) goto end;

	smjs_ses = ses;

	cache_entry_object = get_cache_entry_object(cached);
	args[0] = OBJECT_TO_JSVAL(cache_entry_object);

	if (JS_TRUE == call_script_hook("preformat_html", args, 1, &rval))
		if (JS_FALSE == JSVAL_TO_BOOLEAN(rval))
			ret = EVENT_HOOK_STATUS_LAST;

end:
	smjs_ses = NULL;
	return ret;
}

struct event_hook_info smjs_scripting_hooks[] = {
	{ "pre-format-html", 0, script_hook_pre_format_html, NULL },

	NULL_EVENT_HOOK_INFO,
};
