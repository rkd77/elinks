/* ECMAScript scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "ecmascript/spidermonkey-shared.h"
#include "protocol/uri.h"
#include "main/event.h"
#include "main/module.h"
#include "scripting/smjs/cache_object.h"
#include "scripting/smjs/view_state_object.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/hooks.h"
#include "session/location.h"
#include "session/session.h"
#include "viewer/text/vs.h"


static enum evhook_status
script_hook_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	enum evhook_status ret = EVENT_HOOK_STATUS_NEXT;
	jsval args[1], rval;

	if (*url == NULL) return EVENT_HOOK_STATUS_NEXT;

	smjs_ses = ses;

	args[0] = STRING_TO_JSVAL(JS_NewStringCopyZ(smjs_ctx, *url));

	if (JS_TRUE == smjs_invoke_elinks_object_method(data, args, 1, &rval)) {
		if (JSVAL_IS_BOOLEAN(rval)) {
			if (JS_FALSE == JSVAL_TO_BOOLEAN(rval))
				ret = EVENT_HOOK_STATUS_LAST;
		} else {
			JSString *jsstr = JS_ValueToString(smjs_ctx, rval);
			unsigned char *str = JS_GetStringBytes(jsstr);

			mem_free_set(url, stracpy(str));
		}
	}

	smjs_ses = NULL;

	return ret;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);
	struct cache_entry *cached = va_arg(ap, struct cache_entry *);
	enum evhook_status ret = EVENT_HOOK_STATUS_NEXT;
	JSObject *cache_entry_object, *view_state_object = JSVAL_NULL;
	jsval args[2], rval;

	evhook_use_params(ses && cached);

	if (!smjs_ctx || !cached->length) goto end;

	smjs_ses = ses;

	if (have_location(ses)) {
		struct view_state *vs = &cur_loc(ses)->vs;

		view_state_object = smjs_get_view_state_object(vs);
	}

	cache_entry_object = smjs_get_cache_entry_object(cached);
	if (!cache_entry_object) goto end;

	args[0] = OBJECT_TO_JSVAL(cache_entry_object);
	args[1] = OBJECT_TO_JSVAL(view_state_object);

	if (JS_TRUE == smjs_invoke_elinks_object_method("preformat_html",
	                                                args, 2, &rval))
		if (JS_FALSE == JSVAL_TO_BOOLEAN(rval))
			ret = EVENT_HOOK_STATUS_LAST;

end:
	smjs_ses = NULL;
	return ret;
}

struct event_hook_info smjs_scripting_hooks[] = {
	{ "goto-url", 0, script_hook_url, "goto_url_hook" },
	{ "follow-url", 0, script_hook_url, "follow_url_hook" },
	{ "pre-format-html", 0, script_hook_pre_format_html, NULL },

	NULL_EVENT_HOOK_INFO,
};
