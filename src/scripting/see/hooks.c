/* Ruby scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <see/see.h>

#include "elinks.h"

#include "main/event.h"
#include "protocol/uri.h"
#include "scripting/see/core.h"
#include "scripting/see/hooks.h"
#include "session/location.h"
#include "session/session.h"
#include "util/string.h"


/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

static struct SEE_value *
call_see_hook(struct SEE_interpreter *see, struct session *ses,
	      unsigned char *name,
	      struct SEE_value *args[], int argc,
	      struct SEE_value *result)
{
	struct SEE_string *hook_name = SEE_string_sprintf(see, name);
	struct SEE_value hook;
	SEE_try_context_t try_context;
	struct SEE_value *exception;

	SEE_OBJECT_GET(see, see->Global, hook_name, &hook);

	if (SEE_VALUE_GET_TYPE(&hook) != SEE_OBJECT
	    || !SEE_OBJECT_HAS_CALL(hook.u.object))
		return NULL;

	see_ses = ses;

	SEE_TRY(see, try_context) {
		SEE_OBJECT_CALL(see, hook.u.object, NULL, argc, args, result);
	}

	exception = SEE_CAUGHT(try_context);
	if (exception) {
		SEE_try_context_t try_context2;
		struct SEE_value value;

		SEE_TRY(see, try_context2) {
			struct string error_msg;

			SEE_ToString(see, exception, &value);

			if (init_string(&error_msg)) {
				convert_see_string(&error_msg, value.u.string);
				alert_see_error(ses, error_msg.source);
				done_string(&error_msg);
			}
		}

		if (SEE_CAUGHT(try_context2)) {
			alert_see_error(ses, "exception thrown while printing exception");
		}

		see_ses = NULL;
		return NULL;
	}

	see_ses = NULL;
	return result;
}


static enum evhook_status
script_hook_goto_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	struct SEE_interpreter *see = &see_interpreter;
	struct SEE_value args_[2], *args[2] = { &args_[0], &args_[1] };
	struct SEE_value result;

	if (*url == NULL)
		return EVENT_HOOK_STATUS_NEXT;

	see_ses = ses;
	SEE_SET_STRING(args[0], SEE_string_sprintf(see, "%s", *url));

	if (!ses || !have_location(ses)) {
		SEE_SET_UNDEFINED(args[1]);
	} else {
		SEE_SET_STRING(args[1],
			       SEE_string_sprintf(see, "%s", struri(cur_loc(ses)->vs.uri)));
	}

	if (!call_see_hook(see, ses, "goto_url", args, sizeof_array(args), &result))
		return EVENT_HOOK_STATUS_NEXT;

	switch (SEE_VALUE_GET_TYPE(&result)) {
	case SEE_STRING:
	{
		struct string new_url;

		if (convert_see_string(&new_url, result.u.string))
			mem_free_set(url, new_url.source);
		break;
	}
	case SEE_NULL:
		break;

	default:
		alert_see_error(ses, "goto_url_hook must return a string or null");
	}

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	struct SEE_interpreter *see = &see_interpreter;
	struct SEE_value args_[1], *args[1] = { &args_[0] };
	struct SEE_value result;

	if (*url == NULL)
		return EVENT_HOOK_STATUS_NEXT;

	SEE_SET_STRING(args[0], SEE_string_sprintf(see, "%s", *url));

	if (!call_see_hook(see, ses, "follow_url", args, sizeof_array(args), &result))
		return EVENT_HOOK_STATUS_NEXT;

	switch (SEE_VALUE_GET_TYPE(&result)) {
	case SEE_STRING:
	{
		struct string new_url;

		if (convert_see_string(&new_url, result.u.string))
			mem_free_set(url, new_url.source);
		break;
	}
	case SEE_NULL:
		break;

	default:
		alert_see_error(ses, "follow_url_hook must return a string or null");
	}

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	unsigned char **html = va_arg(ap, unsigned char **);
	int *html_len = va_arg(ap, int *);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *url = va_arg(ap, unsigned char *);
	struct SEE_interpreter *see = &see_interpreter;
	struct SEE_value args_[2], *args[2] = { &args_[0], &args_[1] };
	struct SEE_value result;

	evhook_use_params(url && ses);

	if (*html == NULL || *html_len == 0)
		return EVENT_HOOK_STATUS_NEXT;

	SEE_SET_STRING(args[0], SEE_string_sprintf(see, "%s", url));
	SEE_SET_STRING(args[1], SEE_string_sprintf(see, "%s", *html));

	if (!call_see_hook(see, ses, "pre_format_html", args, sizeof_array(args), &result))
		return EVENT_HOOK_STATUS_NEXT;

	switch (SEE_VALUE_GET_TYPE(&result)) {
	case SEE_STRING:
	{
		struct string new_html;

		if (convert_see_string(&new_html, result.u.string)) {
			mem_free_set(html, new_html.source);
			*html_len = new_html.length;
		}
		break;
	}
	case SEE_NULL:
		break;

	default:
		alert_see_error(ses, "pre_format_hook must return a string or null");
	}

	return EVENT_HOOK_STATUS_NEXT;
}

/* The function can return:
 *  - "PROXY:PORT" to use the specified proxy
 *  - ""           to not use any proxy
 *  - nil          to use the default proxies */
static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	unsigned char **new_proxy_url = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);
	struct SEE_interpreter *see = &see_interpreter;
	struct SEE_value result, args_[1], *args[1] = { &args_[0] };

	if (!new_proxy_url || !url)
		return EVENT_HOOK_STATUS_NEXT;

	SEE_SET_STRING(args[0], SEE_string_sprintf(see, "%s", url));

	if (!call_see_hook(see, NULL, "get_proxy", args, sizeof_array(args), &result))
		return EVENT_HOOK_STATUS_NEXT;

	switch (SEE_VALUE_GET_TYPE(&result)) {
	case SEE_STRING:
	{
		struct string proxy;

		if (convert_see_string(&proxy, result.u.string))
			mem_free_set(new_proxy_url, proxy.source);
		break;
	}
	case SEE_NULL:
		break;

	default:
		alert_see_error(NULL, "proxy_hook must return a string, undefined or null");
	}

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	struct SEE_interpreter *see = &see_interpreter;
	struct SEE_value result;

	call_see_hook(see, NULL, "quit", NULL, 0, &result);

	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info see_scripting_hooks[] = {
	{ "goto-url",        0, script_hook_goto_url,        NULL },
	{ "follow-url",      0, script_hook_follow_url,      NULL },
	{ "pre-format-html", 0, script_hook_pre_format_html, NULL },
	{ "get-proxy",       0, script_hook_get_proxy,       NULL },
	{ "quit",            0, script_hook_quit,            NULL },

	NULL_EVENT_HOOK_INFO,
};
