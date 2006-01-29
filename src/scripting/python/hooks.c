/* Python scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scripting/python/core.h"
#include <Python.h>

#include "elinks.h"

#include "cache/cache.h"
#include "main/event.h"
#include "protocol/uri.h"
#include "scripting/python/hooks.h"
#include "session/location.h"
#include "session/session.h"
#include "util/string.h"


/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

extern PyObject *pDict, *pModule;

static void
do_script_hook_goto_url(struct session *ses, unsigned char **url)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "goto_url_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pValue;
		unsigned char *str;

		if (!ses || !have_location(ses)) {
			str = NULL;
		} else {
			str = struri(cur_loc(ses)->vs.uri);
		}
		pValue = PyObject_CallFunction(pFunc, "s", str);
		if (pValue && (pValue != Py_None)) {
			const unsigned char *res = PyString_AsString(pValue);

			if (res) {
				unsigned char *new_url = stracpy((unsigned char *)res);

				if (new_url) mem_free_set(url, new_url);
			}
			Py_DECREF(pValue);
		} else {
			if (PyErr_Occurred()) {
				PyErr_Print();
				PyErr_Clear();
			}
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
do_script_hook_follow_url(unsigned char **url)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "follow_url_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pValue = PyObject_CallFunction(pFunc, "s", *url);
		if (pValue && (pValue != Py_None)) {
			const unsigned char *str = PyString_AsString(pValue);
			unsigned char *new_url;

			if (str) {
				new_url = stracpy((unsigned char *)str);
				if (new_url) mem_free_set(url, new_url);
			}
			Py_DECREF(pValue);
		} else {
			if (PyErr_Occurred()) {
				PyErr_Print();
				PyErr_Clear();
			}
		}
	}
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);

	if (pDict && *url)
		do_script_hook_follow_url(url);

	return EVENT_HOOK_STATUS_NEXT;
}

static void
do_script_hook_pre_format_html(unsigned char *url, struct cache_entry *cached,
			       struct fragment *fragment)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "pre_format_html_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pValue = PyObject_CallFunction(pFunc, "ss#", url,
		                                         fragment->data,
		                                         fragment->length);

		if (pValue && (pValue != Py_None)) {
			const unsigned char *str = PyString_AsString(pValue);

			if (str) {
				int len = PyString_Size(pValue); /* strlen(str); */

				add_fragment(cached, 0, str, len);
				normalize_cache_entry(cached, len);
			}
			Py_DECREF(pValue);
		} else {
			if (PyErr_Occurred()) {
				PyErr_Print();
				PyErr_Clear();
			}
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
		do_script_hook_pre_format_html(url, cached, fragment);

	return EVENT_HOOK_STATUS_NEXT;
}

static inline void
do_script_hook_get_proxy(unsigned char **new_proxy_url, unsigned char *url)
{
	PyObject *pFunc = PyDict_GetItemString(pDict, "proxy_for_hook");

	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pValue = PyObject_CallFunction(pFunc, "s", url);

		if (pValue && (pValue != Py_None)) {
			const unsigned char *str = PyString_AsString(pValue);

			if (str) {
				unsigned char *new_url = stracpy((unsigned char *)str);

				if (new_url) mem_free_set(new_proxy_url, new_url);
			}
			Py_DECREF(pValue);
		} else {
			if (PyErr_Occurred()) {
				PyErr_Print();
				PyErr_Clear();
			}
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
			if (pValue != Py_None) {
				Py_DECREF(pValue);
			}
		} else {
			if (PyErr_Occurred()) {
				PyErr_Print();
				PyErr_Clear();
			}
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
