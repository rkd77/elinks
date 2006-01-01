/* Guile scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libguile.h>

#include "elinks.h"

#include "cache/cache.h"
#include "main/event.h"
#include "protocol/uri.h"
#include "scripting/guile/hooks.h"
#include "session/session.h"
#include "util/string.h"


static SCM
internal_module(void)
{
	/* XXX: should use internal_module from core.c instead of referring to
	 * the module by name, once I figure out how... --pw */
	return scm_c_resolve_module("elinks internal");
}


/* We need to catch and handle errors because, otherwise, Guile will kill us.
 * Unfortunately, I do not know Guile, but this seems to work... -- Miciah */
static SCM
error_handler(void *data, SCM tag, SCM throw_args)
{
	return SCM_BOOL_F;
}

static SCM
get_guile_hook_do(void *data)
{
	unsigned char *hook = data;

	return scm_c_module_lookup(internal_module(), hook);
}

static SCM
get_guile_hook(unsigned char *hook)
{
	return scm_internal_catch(SCM_BOOL_T, get_guile_hook_do, hook,
				  error_handler, NULL);
}


/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

static enum evhook_status
script_hook_goto_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	SCM proc;
	SCM x;

	evhook_use_params(url && ses);

	if (*url == NULL || !*url[0]) return EVENT_HOOK_STATUS_NEXT;

	proc = get_guile_hook("%goto-url-hook");
	if (SCM_FALSEP(proc)) return EVENT_HOOK_STATUS_NEXT;

	x = scm_call_1(SCM_VARIABLE_REF(proc), scm_makfrom0str(*url));

	if (SCM_STRINGP(x)) {
		unsigned char *new_url;

		new_url = stracpy(SCM_STRING_UCHARS(x));
		if (new_url) {
			mem_free_set(url, new_url);
		}
	} else {
		(*url)[0] = 0;
	}

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	SCM proc;
	SCM x;

	evhook_use_params(url && ses);

	if (*url == NULL || !*url[0]) return EVENT_HOOK_STATUS_NEXT;

	proc = get_guile_hook("%follow-url-hook");
	if (SCM_FALSEP(proc)) return EVENT_HOOK_STATUS_NEXT;

	x = scm_call_1(SCM_VARIABLE_REF(proc), scm_makfrom0str(*url));

	if (SCM_STRINGP(x)) {
		unsigned char *new_url;

		new_url = memacpy(SCM_STRING_UCHARS(x),
				  SCM_STRING_LENGTH(x) + 1);
		if (new_url) {
			mem_free_set(url, new_url);
		}
	} else {
		(*url)[0] = 0;
	}

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);
	struct cache_entry *cached = va_arg(ap, struct cache_entry *);
	struct fragment *fragment = get_cache_fragment(cached);
	unsigned char *url = struri(cached->uri);
	int len;
	SCM proc;
	SCM x;

	evhook_use_params(ses && cached);

	if (!cached->length || !*fragment->data) return EVENT_HOOK_STATUS_NEXT;

	proc = get_guile_hook("%pre-format-html-hook");
	if (SCM_FALSEP(proc)) return EVENT_HOOK_STATUS_NEXT;

	x = scm_call_2(SCM_VARIABLE_REF(proc), scm_makfrom0str(url),
		       scm_mem2string(fragment->data, fragment->length));

	if (!SCM_STRINGP(x)) return EVENT_HOOK_STATUS_NEXT;

	len = SCM_STRING_LENGTH(x);
	add_fragment(cached, 0, SCM_STRING_UCHARS(x), len);
	normalize_cache_entry(cached, len);

	return EVENT_HOOK_STATUS_NEXT;
}

/* The Guile function can return:
 *  - "PROXY:PORT" to use the specified proxy
 *  - ""           to not use any proxy
 *  - nil          to use the default proxies */
static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	unsigned char **retval = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);
	SCM proc = get_guile_hook("%get-proxy-hook");
	SCM x;

	if (SCM_FALSEP(proc)) return EVENT_HOOK_STATUS_NEXT;

	x = scm_call_1(SCM_VARIABLE_REF(proc), scm_makfrom0str(url));

	evhook_use_params(retval && url);

	if (SCM_STRINGP(x)) {
		mem_free_set(retval, memacpy(SCM_STRING_UCHARS(x), SCM_STRING_LENGTH(x)+1));
	} else if (SCM_NULLP(x)) {
		mem_free_set(retval, NULL);
	}

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	SCM proc = get_guile_hook("%quit-hook");

	if (SCM_FALSEP(proc)) return EVENT_HOOK_STATUS_NEXT;

	scm_call_0(SCM_VARIABLE_REF(proc));

	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info guile_scripting_hooks[] = {
	{ "goto-url", 0, script_hook_goto_url, NULL },
	{ "follow-url", 0, script_hook_follow_url, NULL },
	{ "pre-format-html", 0, script_hook_pre_format_html, NULL },
	{ "get-proxy", 0, script_hook_get_proxy, NULL },
	{ "quit", 0, script_hook_quit, NULL },
	NULL_EVENT_HOOK_INFO,
};
