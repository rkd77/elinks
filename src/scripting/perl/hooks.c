/* Perl scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "main/event.h"
#include "protocol/uri.h"
#include "scripting/perl/core.h"
#include "scripting/perl/hooks.h"
#include "session/location.h"
#include "session/session.h"
#include "util/string.h"

#define my_XPUSHs(s, slen) XPUSHs(sv_2mortal(newSVpvn(s, slen)))

/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

static inline void
do_script_hook_goto_url(struct session *ses, char **url)
{
	ELOG
	int count;
	dSP;	/* Keep in variables declaration block. */

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	my_XPUSHs(*url, strlen(*url));
	if (!ses || !have_location(ses)) {
		XPUSHs(sv_2mortal(newSV(0)));
	} else {
		char *uri = struri(cur_loc(ses)->vs.uri);

		my_XPUSHs(uri, strlen(uri));
	}
	PUTBACK;

	count = call_pv("goto_url_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) count = 0;	/* FIXME: error message ? */
	SPAGAIN;
	if (count == 1) {
#ifndef CONFIG_PERL_POPPX_WITHOUT_N_A
		STRLEN n_a;	/* Used by POPpx macro. */
#endif
		char *new_url = POPpx;

		if (new_url) {
			char *n = stracpy(new_url);

			if (n) {
				mem_free_set(url, n);
			}
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static enum evhook_status
script_hook_goto_url(va_list ap, void *data)
{
	ELOG
	char **url = va_arg(ap, char **);
	struct session *ses = va_arg(ap, struct session *);

	if (my_perl && *url)
		do_script_hook_goto_url(ses, url);

	return EVENT_HOOK_STATUS_NEXT;
}

static inline void
do_script_hook_follow_url(char **url)
{
	ELOG
	int count;
	dSP;	/* Keep in variables declaration block. */

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	my_XPUSHs(*url, strlen(*url));
	PUTBACK;

	count = call_pv("follow_url_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) count = 0;	/* FIXME: error message ? */
	SPAGAIN;
	if (count == 1) {
#ifndef CONFIG_PERL_POPPX_WITHOUT_N_A
		STRLEN n_a;	/* Used by POPpx macro. */
#endif
		char *new_url = POPpx;

		if (new_url) {
			char *n = stracpy(new_url);

			if (n) {
				mem_free_set(url, n);
			}
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	ELOG
	char **url = va_arg(ap, char **);

	if (my_perl && *url)
		do_script_hook_follow_url(url);

	return EVENT_HOOK_STATUS_NEXT;
}

static inline void
do_script_hook_pre_format_html(char *url, struct cache_entry *cached,
			       struct fragment *fragment)
{
	ELOG
	int count;
	dSP;	/* Keep in variables declaration block. */

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	my_XPUSHs(url, strlen(url));
	my_XPUSHs(fragment->data, fragment->length);
	PUTBACK;

	count = call_pv("pre_format_html_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) count = 0;	/* FIXME: error message ? */
	SPAGAIN;
	if (count == 1) {
		SV *new_html_sv = POPs;
		STRLEN new_html_len;
		char *new_html = SvPV(new_html_sv, new_html_len);

		if (new_html) {
			add_fragment(cached, 0, new_html, new_html_len);
			normalize_cache_entry(cached, new_html_len);
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	ELOG
	struct session *ses = va_arg(ap, struct session *);
	struct cache_entry *cached = va_arg(ap, struct cache_entry *);
	struct fragment *fragment = get_cache_fragment(cached);
	char *url = struri(cached->uri);

	evhook_use_params(ses && cached);

	if (my_perl && url && cached->length && *fragment->data)
		do_script_hook_pre_format_html(url, cached, fragment);

	return EVENT_HOOK_STATUS_NEXT;
}

static inline void
do_script_hook_get_proxy(char **new_proxy_url, char *url)
{
	ELOG
	int count;
	dSP;	/* Keep in variables declaration block. */

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	my_XPUSHs(url, strlen(url));
	PUTBACK;

	count = call_pv("proxy_for_hook", G_EVAL | G_SCALAR);
	if (SvTRUE(ERRSV)) count = 0;	/* FIXME: error message ? */
	SPAGAIN;
	if (count == 1) {
		if (TOPs == &PL_sv_undef) {
			(void) POPs;
			mem_free_set(new_proxy_url, NULL);
		} else {
#ifndef CONFIG_PERL_POPPX_WITHOUT_N_A
			STRLEN n_a; /* Used by POPpx macro. */
#endif
			char *new_url = POPpx;

			mem_free_set(new_proxy_url, stracpy(new_url));
		}
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
}

static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	ELOG
	char **new_proxy_url = va_arg(ap, char **);
	char *url = va_arg(ap, char *);

	if (my_perl && new_proxy_url && url)
		do_script_hook_get_proxy(new_proxy_url, url);

	return EVENT_HOOK_STATUS_NEXT;
}

static inline void
do_script_hook_quit(void)
{
	ELOG
	dSP;

	PUSHMARK(SP);

	call_pv("quit_hook", G_EVAL | G_DISCARD | G_NOARGS);
}

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	ELOG
	if (my_perl)
		do_script_hook_quit();

	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info perl_scripting_hooks[] = {
	{ "goto-url", 0, script_hook_goto_url, {NULL} },
	{ "follow-url", 0, script_hook_follow_url, {NULL} },
	{ "pre-format-html", 0, script_hook_pre_format_html, {NULL} },
	{ "get-proxy", 0, script_hook_get_proxy, {NULL} },
	{ "quit", 0, script_hook_quit, {NULL} },
	NULL_EVENT_HOOK_INFO,
};
