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
	char **url = va_arg(ap, char **);
	struct session *ses = va_arg(ap, struct session *);
	enum evhook_status ret = EVENT_HOOK_STATUS_NEXT;

	JS::Value args[3];
	JS::RootedValue r_rval(smjs_ctx);

	if (*url == NULL) return EVENT_HOOK_STATUS_NEXT;

	JS::Realm *prev = JS::EnterRealm(smjs_ctx, smjs_elinks_object);

	smjs_ses = ses;
	args[2].setString(JS_NewStringCopyZ(smjs_ctx, *url));

	if (true == smjs_invoke_elinks_object_method(data, 1, args, &r_rval)) {
		if (r_rval.isBoolean()) {
			if (false == (r_rval.toBoolean()))
				ret = EVENT_HOOK_STATUS_LAST;
		} else {
			char *str = jsval_to_string(smjs_ctx, r_rval);

			mem_free_set(url, stracpy(str));
		}
	}

	smjs_ses = NULL;
	JS::LeaveRealm(smjs_ctx, prev);

	return ret;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);
	struct cache_entry *cached = va_arg(ap, struct cache_entry *);
	enum evhook_status ret = EVENT_HOOK_STATUS_NEXT;
	JSObject *cache_entry_object, *view_state_object = NULL;
	JS::Value args[4];
	JS::RootedValue r_rval(smjs_ctx);

	JS::Realm *prev = JS::EnterRealm(smjs_ctx, smjs_elinks_object);

	evhook_use_params(ses && cached);

	if (!smjs_ctx || !cached->length) {
		goto end;
	}

	smjs_ses = ses;


	if (ses && have_location(ses)) {
		struct view_state *vs = &cur_loc(ses)->vs;

		view_state_object = smjs_get_view_state_object(vs);
	}

	cache_entry_object = smjs_get_cache_entry_object(cached);
	if (!cache_entry_object) goto end;

	args[2].setObject(*cache_entry_object);
	args[3].setObject(*view_state_object);

	if (true == smjs_invoke_elinks_object_method("preformat_html",
	                                                2, args, &r_rval)) {
		if (false == r_rval.toBoolean())
			ret = EVENT_HOOK_STATUS_LAST;
	}


end:
	JS::LeaveRealm(smjs_ctx, prev);
	smjs_ses = NULL;
	return ret;
}

struct event_hook_info smjs_scripting_hooks[] = {
	{ "goto-url", 0, script_hook_url, {"goto_url_hook"} },
	{ "follow-url", 0, script_hook_url, {"follow_url_hook"} },
	{ "pre-format-html", 0, script_hook_pre_format_html, {NULL} },

	NULL_EVENT_HOOK_INFO,
};
