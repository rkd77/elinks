/* Python scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "elinks.h"

#include "cache/cache.h"
#include "main/event.h"
#include "protocol/uri.h"
#include "scripting/python/core.h"
#include "scripting/python/hooks.h"
#include "session/location.h"
#include "session/session.h"
#include "util/string.h"

#undef _POSIX_C_SOURCE
#include <Python.h>

/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

extern PyObject *pDict;
extern PyObject *pModule;

static void
do_script_hook_goto_url(struct session *ses, unsigned char **url)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "goto_url_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pValue;
		unsigned char *current_url;

		if (!ses || !have_location(ses)) {
			current_url = NULL;
		} else {
			current_url = struri(cur_loc(ses)->vs.uri);
		}

		pValue = PyObject_CallFunction(pFunc, "ss", *url, current_url);
		if (pValue) {
			if (pValue != Py_None) {
				const unsigned char *str;
				unsigned char *new_url;

				str = PyString_AsString(pValue);
				if (str) {
					new_url = stracpy((unsigned char *)str);
					if (new_url) mem_free_set(url, new_url);
				}
			}
			Py_DECREF(pValue);
		} else {
			alert_python_error(ses);
		}
	}
}

static enum evhook_status
script_hook_goto_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);

	if (pDict && *url)
		do_script_hook_goto_url(ses, url);

	return EVENT_HOOK_STATUS_NEXT;
}

static void
do_script_hook_follow_url(struct session *ses, unsigned char **url)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "follow_url_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pValue = PyObject_CallFunction(pFunc, "s", *url);
		if (pValue) {
			if (pValue != Py_None) {
				const unsigned char *str;
				unsigned char *new_url;

				str = PyString_AsString(pValue);
				if (str) {
					new_url = stracpy((unsigned char *)str);
					if (new_url) mem_free_set(url, new_url);
				}
			}
			Py_DECREF(pValue);
		} else {
			alert_python_error(ses);
		}
	}
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);

	if (pDict && *url)
		do_script_hook_follow_url(ses, url);

	return EVENT_HOOK_STATUS_NEXT;
}

static void
do_script_hook_pre_format_html(struct session *ses, unsigned char *url,
			       struct cache_entry *cached,
			       struct fragment *fragment)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "pre_format_html_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pValue = PyObject_CallFunction(pFunc, "ss#", url,
		                                         fragment->data,
		                                         fragment->length);

		if (pValue) {
			if (pValue != Py_None) {
				const unsigned char *str;
				int len;

				str = PyString_AsString(pValue);
				if (str) {
					len = PyString_Size(pValue);
					add_fragment(cached, 0, str, len);
					normalize_cache_entry(cached, len);
				}
			}
			Py_DECREF(pValue);
		} else {
			alert_python_error(ses);
		}
	}
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);
	struct cache_entry *cached = va_arg(ap, struct cache_entry *);
	struct fragment *fragment = get_cache_fragment(cached);
	unsigned char *url = struri(cached->uri);

	if (pDict && ses && url && cached->length && *fragment->data)
		do_script_hook_pre_format_html(ses, url, cached, fragment);

	return EVENT_HOOK_STATUS_NEXT;
}

static inline void
do_script_hook_get_proxy(unsigned char **new_proxy_url, unsigned char *url)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "proxy_for_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pValue = PyObject_CallFunction(pFunc, "s", url);

		if (pValue) {
			if (pValue != Py_None) {
				const unsigned char *str;
				unsigned char *new_url;

				str = PyString_AsString(pValue);
				if (str) {
					new_url = stracpy((unsigned char *)str);
					if (new_url) mem_free_set(new_proxy_url,
								  new_url);
				}
			}
			Py_DECREF(pValue);
		} else {
			alert_python_error(NULL);
		}
	}
}

static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	unsigned char **new_proxy_url = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);

	if (pDict && new_proxy_url && url)
		do_script_hook_get_proxy(new_proxy_url, url);

	return EVENT_HOOK_STATUS_NEXT;
}

static void
do_script_hook_quit(void)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "quit_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pValue = PyObject_CallFunction(pFunc, NULL);

		if (pValue) {
			Py_DECREF(pValue);
		} else {
			alert_python_error(NULL);
		}
	}
}

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	if (pDict) do_script_hook_quit();
	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info python_scripting_hooks[] = {
	{ "goto-url", 0, script_hook_goto_url, NULL },
	{ "follow-url", 0, script_hook_follow_url, NULL },
	{ "pre-format-html", 0, script_hook_pre_format_html, NULL },
	{ "get-proxy", 0, script_hook_get_proxy, NULL },
	{ "quit", 0, script_hook_quit, NULL },
	NULL_EVENT_HOOK_INFO,
};
