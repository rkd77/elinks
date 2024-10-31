/* Python scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scripting/python/pythoninc.h"

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
replace_with_python_string(char **dest, PyObject *object)
{
	char *str;

	if (object == Py_None) {
		return object;
	}

	str = (char *) PyUnicode_AsUTF8(object);

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
	char **url = va_arg(ap, char **);
	struct session *ses = va_arg(ap, struct session *);
	char *method = (char *)data;
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
get_codepage(char *head)
{
	int cp_index = -1;
	char *part = head;

	if (!head) {
		goto none;
	}
	while (cp_index == -1) {
		char *ct_charset;
		/* scan_http_equiv() appends the meta http-equiv directives to
		 * the protocol header before this function is called, but the
		 * HTTP Content-Type header has precedence, so the HTTP header
		 * will be used if it exists and the meta header is only used
		 * as a fallback.  See bug 983.  */
		char *a = parse_header(part, "Content-Type", &part);

		if (!a) break;

		parse_header_param(a, "charset", &ct_charset, 0);
		if (ct_charset) {
			cp_index = get_cp_index(ct_charset);
			mem_free(ct_charset);
		}
		mem_free(a);
	}

	if (cp_index == -1) {
		char *a = parse_header(head, "Content-Charset", NULL);

		if (a) {
			cp_index = get_cp_index(a);
			mem_free(a);
		}
	}

	if (cp_index == -1) {
		char *a = parse_header(head, "Charset", NULL);

		if (a) {
			cp_index = get_cp_index(a);
			mem_free(a);
		}
	}

none:
	if (cp_index == -1) {
		cp_index = get_cp_index("utf-8");
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
	char *url = struri(cached->uri);
	int codepage = get_codepage(cached->head);
	int utf8_cp = get_cp_index("utf-8");
	const char *method = "pre_format_html_hook";
	struct session *saved_python_ses = python_ses;
	PyObject *result = NULL;
	int success = 0;

	evhook_use_params(ses && cached);

	if (!python_hooks || !cached->length || !*fragment->data
	    || !PyObject_HasAttrString(python_hooks, method))
		return EVENT_HOOK_STATUS_NEXT;

	python_ses = ses;

	if (!is_cp_utf8(codepage)) {
		int width;
		struct conv_table *ctable = get_translation_table(codepage, utf8_cp);
		char *utf8_data = convert_string(ctable, fragment->data, (int)fragment->length, utf8_cp, CSM_NONE,
			&width, NULL, NULL);

		if (!utf8_data) {
			goto error;
		}
		result = PyObject_CallMethod(python_hooks, method, "ss#", url, utf8_data, width);
		mem_free(utf8_data);
	} else {
		char *utf8_data = mem_calloc(1, fragment->length + 1);

		if (!utf8_data) {
			goto error;
		}
		iconv_t cd = iconv_open("UTF-8//IGNORE", "UTF-8");

		if (cd == (iconv_t)-1) {
			mem_free(utf8_data);
			goto error;
		}
		char *inbuf = fragment->data;
		char *outbuf = utf8_data;
		size_t inbytes_len = fragment->length;
		size_t outbytes_len = fragment->length;

		if (iconv(cd, &inbuf, &inbytes_len, &outbuf, &outbytes_len) < 0) {
			mem_free(utf8_data);
			iconv_close(cd);
			goto error;
		}
		int len = (int)(fragment->length - outbytes_len);
		result = PyObject_CallMethod(python_hooks, method, "ss#", url, utf8_data, len);

		if (result == Py_None && outbytes_len > 0) {
			(void)add_fragment(cached, 0, utf8_data, len);
			normalize_cache_entry(cached, len);
		}
		iconv_close(cd);
		mem_free(utf8_data);
	}

	if (!result) goto error;

	if (result != Py_None) {
		const char *str;
		Py_ssize_t len;

		str = PyUnicode_AsUTF8AndSize(result, &len);

		if (!str) {
			goto error;
		}

		if (!is_cp_utf8(codepage)) {
			int width;
			struct conv_table *ctable = get_translation_table(utf8_cp, codepage);
			char *dec_data = convert_string(ctable, str, (int)len, codepage, CSM_NONE,
				&width, NULL, NULL);

			if (!dec_data) {
				goto error;
			}
			(void) add_fragment(cached, 0, dec_data, width);
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
	char **proxy = va_arg(ap, char **);
	char *url = va_arg(ap, char *);
	const char *method = "proxy_for_hook";
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
	const char *method = "quit_hook";
	PyObject *result;

	if (!python_hooks || !PyObject_HasAttrString(python_hooks, method))
		return EVENT_HOOK_STATUS_NEXT;

	result = PyObject_CallMethod(python_hooks, method, NULL);
	if (!result) alert_python_error();

	Py_XDECREF(result);

	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info python_scripting_hooks[] = {
	{ "goto-url", 0, script_hook_url, {"goto_url_hook"} },
	{ "follow-url", 0, script_hook_url, {"follow_url_hook"} },
	{ "pre-format-html", 0, script_hook_pre_format_html, {NULL} },
	{ "get-proxy", 0, script_hook_get_proxy, {NULL} },
	{ "quit", 0, script_hook_quit, {NULL} },
	NULL_EVENT_HOOK_INFO,
};
