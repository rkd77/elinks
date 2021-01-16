/* Python scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <iconv.h>
#include <stdarg.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "main/event.h"
#include "protocol/header.h"
#include "protocol/uri.h"
#include "scripting/python/core.h"
#include "session/session.h"
#include "util/memory.h"
#include "util/string.h"

extern PyObject *python_hooks;

/*
 * A utility function for script_hook_url() and script_hook_get_proxy():
 * Free a char * and replace it with the contents of a Python string.
 * (Py_None is ignored.)
 */

static PyObject *
replace_with_python_string(unsigned char **dest, PyObject *object)
{
	unsigned char *str;

	if (object == Py_None) {
		return object;
	}

	str = (unsigned char *) PyUnicode_AsUTF8(object);

	if (!str) return NULL;

	str = stracpy(str);
	if (!str) return PyErr_NoMemory();

	mem_free_set(dest, str);
	return object;
}

/* Call a Python hook for a goto-url or follow-url event. */

static enum evhook_status
script_hook_url(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	char *method = data;
	struct session *saved_python_ses = python_ses;
	PyObject *result;

	evhook_use_params(url && ses);

	if (!python_hooks || !url || !*url
	    || !PyObject_HasAttrString(python_hooks, method))
		return EVENT_HOOK_STATUS_NEXT;

	python_ses = ses;

	result = PyObject_CallMethod(python_hooks, method, "s", *url);

	if (!result || !replace_with_python_string(url, result))
		alert_python_error();

	Py_XDECREF(result);

	python_ses = saved_python_ses;

	return EVENT_HOOK_STATUS_NEXT;
}

static int
get_codepage(unsigned char *head)
{
	int cp_index = -1;
	unsigned char *part = head;

	if (!head) {
		goto none;
	}
	while (cp_index == -1) {
		unsigned char *ct_charset;
		/* scan_http_equiv() appends the meta http-equiv directives to
		 * the protocol header before this function is called, but the
		 * HTTP Content-Type header has precedence, so the HTTP header
		 * will be used if it exists and the meta header is only used
		 * as a fallback.  See bug 983.  */
		unsigned char *a = parse_header(part, "Content-Type", &part);

		if (!a) break;

		parse_header_param(a, "charset", &ct_charset, 0);
		if (ct_charset) {
			cp_index = get_cp_index(ct_charset);
			mem_free(ct_charset);
		}
		mem_free(a);
	}

	if (cp_index == -1) {
		unsigned char *a = parse_header(head, "Content-Charset", NULL);

		if (a) {
			cp_index = get_cp_index(a);
			mem_free(a);
		}
	}

	if (cp_index == -1) {
		unsigned char *a = parse_header(head, "Charset", NULL);

		if (a) {
			cp_index = get_cp_index(a);
			mem_free(a);
		}
	}

none:
	if (cp_index == -1) {
		cp_index = get_cp_index("ISO-8859-1");
	}

	return cp_index;
}

/* Call a Python hook for a pre-format-html event. */

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);
	struct cache_entry *cached = va_arg(ap, struct cache_entry *);
	struct fragment *fragment = get_cache_fragment(cached);
	unsigned char *url = struri(cached->uri);
	int codepage = get_codepage(cached->head);
	char *method = "pre_format_html_hook";
	struct session *saved_python_ses = python_ses;
	PyObject *result = NULL;
	int success = 0;

	evhook_use_params(ses && cached);

	if (!python_hooks || !cached->length || !*fragment->data
	    || !PyObject_HasAttrString(python_hooks, method))
		return EVENT_HOOK_STATUS_NEXT;

	python_ses = ses;

	if (!is_cp_utf8(codepage)) {
		size_t iconv_res;
		size_t ileft;
		size_t oleft;
		char *inbuf, *outbuf;
		char *utf8_data = mem_alloc(fragment->length * 8);
		iconv_t cd;

		if (!utf8_data) {
			goto error;
		}
		cd = iconv_open("utf-8", get_cp_mime_name(codepage));
		if (cd == (iconv_t)-1) {
			mem_free(utf8_data);
			goto error;
		}
		inbuf = fragment->data;
		outbuf = utf8_data;
		ileft = fragment->length;
		oleft = fragment->length * 8;
		iconv_res = iconv(cd, &inbuf, &ileft, &outbuf, &oleft);

		if (iconv_res == -1) {
			mem_free(utf8_data);
			goto error;
		}
		iconv_close(cd);

		result = PyObject_CallMethod(python_hooks, method, "ss#", url, utf8_data, fragment->length * 8 - oleft);
		mem_free(utf8_data);
	} else {
		result = PyObject_CallMethod(python_hooks, method, "ss#", url, fragment->data, fragment->length);
	}
	if (!result) goto error;

	if (result != Py_None) {
		const unsigned char *str;
		Py_ssize_t len;

		str = PyUnicode_AsUTF8AndSize(result, &len);
		if (!str) {
			goto error;
		}

		if (!is_cp_utf8(codepage)) {
			size_t iconv_res;
			size_t ileft;
			size_t oleft;
			char *inbuf, *outbuf;
			char *dec_data = mem_alloc(len * 4);
			iconv_t cd;

			if (!dec_data) {
				goto error;
			}
			cd = iconv_open(get_cp_mime_name(codepage), "utf-8");
			if (cd == (iconv_t)-1) {
				mem_free(dec_data);
				goto error;
			}
			inbuf = (char *)str;
			outbuf = dec_data;
			ileft = len;
			oleft = len * 4;
			iconv_res = iconv(cd, &inbuf, &ileft, &outbuf, &oleft);

			if (iconv_res == -1) {
				mem_free(dec_data);
				goto error;
			}
			iconv_close(cd);
			(void) add_fragment(cached, 0, dec_data, len * 4 - oleft);
			mem_free(dec_data);
		} else {
			/* This assumes the Py_ssize_t len is not too large to
			 * fit in the off_t parameter of normalize_cache_entry().
			 * add_fragment() itself seems to assume the same thing,
			 * and there is no standard OFF_MAX macro against which
			* ELinks could check the value.  */
			(void) add_fragment(cached, 0, str, len);
		}
		normalize_cache_entry(cached, len);
	}

	success = 1;

error:
	if (!success) alert_python_error();

	Py_XDECREF(result);

	python_ses = saved_python_ses;

	return EVENT_HOOK_STATUS_NEXT;
}

/* Call a Python hook for a get-proxy event. */

static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	unsigned char **proxy = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);
	char *method = "proxy_for_hook";
	PyObject *result;

	evhook_use_params(proxy && url);

	if (!python_hooks || !proxy || !url
	    || !PyObject_HasAttrString(python_hooks, method))
		return EVENT_HOOK_STATUS_NEXT;

	result = PyObject_CallMethod(python_hooks, method, "s", url);

	if (!result || !replace_with_python_string(proxy, result))
		alert_python_error();

	Py_XDECREF(result);

	return EVENT_HOOK_STATUS_NEXT;
}

/* Call a Python hook for a quit event. */

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	char *method = "quit_hook";
	PyObject *result;

	if (!python_hooks || !PyObject_HasAttrString(python_hooks, method))
		return EVENT_HOOK_STATUS_NEXT;

	result = PyObject_CallMethod(python_hooks, method, NULL);
	if (!result) alert_python_error();

	Py_XDECREF(result);

	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info python_scripting_hooks[] = {
	{ "goto-url", 0, script_hook_url, "goto_url_hook" },
	{ "follow-url", 0, script_hook_url, "follow_url_hook" },
	{ "pre-format-html", 0, script_hook_pre_format_html, NULL },
	{ "get-proxy", 0, script_hook_get_proxy, NULL },
	{ "quit", 0, script_hook_quit, NULL },
	NULL_EVENT_HOOK_INFO,
};
