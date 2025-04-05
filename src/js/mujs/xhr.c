/* The MuJS xhr object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBCURL
#include <curl/curl.h>
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/mujs/mapa.h"
#include "js/mujs.h"
#include "js/mujs/xhr.h"
#include "js/timer.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"


const unsigned short UNSENT = 0;
const unsigned short OPENED = 1;
const unsigned short HEADERS_RECEIVED = 2;
const unsigned short LOADING = 3;
const unsigned short DONE = 4;

static void mjs_xhr_get_property_onabort(js_State *J);
static void mjs_xhr_get_property_onerror(js_State *J);
static void mjs_xhr_get_property_onload(js_State *J);
static void mjs_xhr_get_property_onloadend(js_State *J);
static void mjs_xhr_get_property_onloadstart(js_State *J);
static void mjs_xhr_get_property_onprogress(js_State *J);
static void mjs_xhr_get_property_onreadystatechange(js_State *J);
static void mjs_xhr_get_property_ontimeout(js_State *J);

static void mjs_xhr_get_property_readyState(js_State *J);
static void mjs_xhr_get_property_response(js_State *J);
static void mjs_xhr_get_property_responseText(js_State *J);
static void mjs_xhr_get_property_responseType(js_State *J);
static void mjs_xhr_get_property_responseURL(js_State *J);
static void mjs_xhr_get_property_status(js_State *J);
static void mjs_xhr_get_property_statusText(js_State *J);
static void mjs_xhr_get_property_timeout(js_State *J);
static void mjs_xhr_get_property_upload(js_State *J);
static void mjs_xhr_get_property_withCredentials(js_State *J);

static void mjs_xhr_set_property_onabort(js_State *J);
static void mjs_xhr_set_property_onerror(js_State *J);
static void mjs_xhr_set_property_onload(js_State *J);
static void mjs_xhr_set_property_onloadend(js_State *J);
static void mjs_xhr_set_property_onloadstart(js_State *J);
static void mjs_xhr_set_property_onprogress(js_State *J);
static void mjs_xhr_set_property_onreadystatechange(js_State *J);
static void mjs_xhr_set_property_ontimeout(js_State *J);

static void mjs_xhr_set_property_responseType(js_State *J);
static void mjs_xhr_set_property_timeout(js_State *J);
static void mjs_xhr_set_property_withCredentials(js_State *J);

static void onload_run(void *data);
static void onloadend_run(void *data);
static void onreadystatechange_run(void *data);
static void ontimeout_run(void *data);

char *
normalize(char *value)
{
	char *ret = value;
	size_t index = strspn(ret, "\r\n\t ");
	ret += index;
	char *end = strchr(ret, 0);

	do {
		--end;

		if (*end == '\r' || *end == '\n' || *end == '\t' || *end == ' ') {
			*end = '\0';
		} else {
			break;
		}
	} while (end > ret);

	return ret;
}

static void
mjs_xhr_finalizer(js_State *J, void *data)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)data;

	if (xhr) {
		if (xhr->uri) {
			done_uri(xhr->uri);
		}
		mem_free_if(xhr->response);
		mem_free_if(xhr->responseText);
		mem_free_if(xhr->responseType);
		mem_free_if(xhr->responseURL);
		mem_free_if(xhr->statusText);
		mem_free_if(xhr->upload);

		if (xhr->onabort) js_unref(J, xhr->onabort);
		if (xhr->onerror) js_unref(J, xhr->onerror);
		if (xhr->onload) js_unref(J, xhr->onload);
		if (xhr->onloadend) js_unref(J, xhr->onloadend);
		if (xhr->onloadstart) js_unref(J, xhr->onloadstart);
		if (xhr->onprogress) js_unref(J, xhr->onprogress);
		if (xhr->onreadystatechange) js_unref(J, xhr->onreadystatechange);
		if (xhr->ontimeout) js_unref(J, xhr->ontimeout);
		if (xhr->thisval) js_unref(J, xhr->thisval);

		struct xhr_listener *l;

		foreach(l, xhr->listeners) {
			mem_free_set(&l->typ, NULL);
			if (l->fun) js_unref(J, l->fun);
		}
		free_list(xhr->listeners);

		delete_map_str(xhr->responseHeaders);
		delete_map_str(xhr->requestHeaders);

		mem_free(xhr);
	}
}

static void
mjs_xhr_static_get_property_UNSENT(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, UNSENT);
}

static void
mjs_xhr_static_get_property_OPENED(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, OPENED);
}

static void
mjs_xhr_static_get_property_HEADERS_RECEIVED(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, HEADERS_RECEIVED);
}

static void
mjs_xhr_static_get_property_LOADING(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, LOADING);
}


static void
mjs_xhr_static_get_property_DONE(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, DONE);
}

static void mjs_xhr_abort(js_State *J);
static void mjs_xhr_addEventListener(js_State *J);
static void mjs_xhr_getAllResponseHeaders(js_State *J);
static void mjs_xhr_getResponseHeader(js_State *J);
static void mjs_xhr_open(js_State *J);
static void mjs_xhr_overrideMimeType(js_State *J);
static void mjs_xhr_removeEventListener(js_State *J);
static void mjs_xhr_send(js_State *J);
static void mjs_xhr_setRequestHeader(js_State *J);

static void
mjs_xhr_abort(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (xhr && xhr->download.conn) {
		abort_connection(xhr->download.conn, connection_state(S_INTERRUPTED));
	}
	js_pushundefined(J);
}

static void
mjs_xhr_addEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_error(J, "out of memory");
		return;
	}
	js_copy(J, 2);
	const char *fun = js_ref(J);

	struct xhr_listener *l;

	foreach(l, xhr->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (!strcmp(l->fun, fun)) {
			mem_free(method);
			js_pushundefined(J);
			return;
		}
	}
	struct xhr_listener *n = (struct xhr_listener *)mem_calloc(1, sizeof(*n));

	if (n) {
		n->typ = method;
		n->fun = fun;
		add_to_list_end(xhr->listeners, n);
	}
	js_pushundefined(J);
}

static void
mjs_xhr_removeEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_error(J, "out of memory");
		return;
	}
	js_copy(J, 2);
	const char *fun = js_ref(J);
	struct xhr_listener *l;

	foreach(l, xhr->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (!strcmp(l->fun, fun)) {
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			if (l->fun) js_unref(J, l->fun);
			mem_free(l);
			mem_free(method);
			js_pushundefined(J);
			return;
		}
	}
	mem_free(method);
	js_pushundefined(J);
}

static void
mjs_xhr_getAllResponseHeaders(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	char *output = get_output_headers(xhr);

	if (!output) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, output);
	mem_free(output);
}

static void
mjs_xhr_getResponseHeader(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	const char *header = js_tostring(J, 1);

	if (header) {
		char *output = get_output_header(header, xhr);

		if (output) {
			js_pushstring(J, output);
			mem_free(output);
			return;
		}
	}
	js_pushnull(J);
}

static void
mjs_xhr_open(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	const char *method = js_tostring(J, 1);

	if (!method) {
		js_error(J, "!method");
		return;
	}

	const char *allowed[] = { "", "GET", "HEAD", "POST", NULL };
	bool method_ok = false;

	for (int i = 1; allowed[i]; i++) {
		if (!strcasecmp(allowed[i], method)) {
			method_ok = true;
			xhr->method = i;
			break;
		}
	}

	if (!method_ok) {
		js_pushnull(J);
		return;
	}
	mem_free_set(&xhr->responseURL, null_or_stracpy(js_tostring(J, 2)));

	if (!xhr->responseURL) {
		js_pushnull(J);
		return;
	}

	if (xhr->responseURL[0] != '/') {
		char *ref = get_uri_string(vs->uri, URI_DIR_LOCATION | URI_PATH);

		if (ref) {
			char *slash = strrchr(ref, '/');

			if (slash) {
				*slash = '\0';
			}
			char *url = straconcat(ref, "/", xhr->responseURL, NULL);

			if (url) {
				xhr->uri = get_uri(url, URI_NONE);
				mem_free(url);
			}
			mem_free(ref);
		}
	} else {
		char *ref = get_uri_string(vs->uri, URI_SERVER);

		if (ref) {
			char *url = straconcat(ref, xhr->responseURL, NULL);

			if (url) {
				xhr->uri = get_uri(url, URI_NONE);
				mem_free(url);
			}
			mem_free(ref);
		}
	}

	if (!xhr->uri) {
		xhr->uri = get_uri(xhr->responseURL, URI_NONE);
	}

	if (!xhr->uri) {
		js_pushnull(J);
		return;
	}

	xhr->async = js_isundefined(J, 3) ? true : js_toboolean(J, 3);
	const char *username = js_isundefined(J, 4) ? NULL : js_tostring(J, 4);
	const char *password = js_isundefined(J, 5) ? NULL : js_tostring(J, 5);

	if (username || password) {
		if (username) {
			xhr->uri->user = (char *)username;
			xhr->uri->userlen = strlen(username);
		}
		if (password) {
			xhr->uri->password = (char *)password;
			xhr->uri->passwordlen = strlen(password);
		}
		char *url2 = get_uri_string(xhr->uri, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD);

		if (!url2) {
			js_error(J, "!url2");
			return;
		}
		done_uri(xhr->uri);
		xhr->uri = get_uri(url2, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD);
		mem_free(url2);
	}
	if (!xhr->async && (xhr->timeout || (xhr->responseType && *(xhr->responseType)))) {
		js_pushnull(J);
		return;
	}

	// TODO terminate fetch
	xhr->isSend = false;
	xhr->isUpload = false;

	delete_map_str(xhr->requestHeaders);
	delete_map_str(xhr->responseHeaders);
	xhr->requestHeaders = attr_create_new_requestHeaders_map();
	xhr->responseHeaders = attr_create_new_responseHeaders_map();

	mem_free_set(&xhr->response, NULL);
	mem_free_set(&xhr->responseText, NULL);
	xhr->responseLength = 0;

	if (xhr->readyState != OPENED) {
		xhr->readyState = OPENED;
		register_bottom_half(onreadystatechange_run, xhr);
	}
	js_pushundefined(J);
}

static void
mjs_xhr_overrideMimeType(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
/// TODO
	js_pushundefined(J);
}

static void
onload_run(void *data)
{
	struct mjs_xhr *xhr = (struct mjs_xhr *)data;

	if (xhr && ecmascript_found(xhr->interpreter)) {
		struct ecmascript_interpreter *interpreter = xhr->interpreter;
		js_State *J = (js_State *)interpreter->backend_data;

		struct xhr_listener *l;

		foreach(l, xhr->listeners) {
			if (strcmp(l->typ, "load")) {
				continue;
			}
			js_getregistry(J, l->fun); /* retrieve the js function from the registry */
			js_getregistry(J, xhr->thisval);
			js_pcall(J, 0);
			js_pop(J, 1);
		}

		if (xhr->onload) {
			js_getregistry(J, xhr->onload); /* retrieve the js function from the registry */
			js_getregistry(J, xhr->thisval);
			js_pcall(J, 0);
			js_pop(J, 1);
		}
		check_for_rerender(interpreter, "xhr_onload");
	}
}

static void
onloadend_run(void *data)
{
	struct mjs_xhr *xhr = (struct mjs_xhr *)data;

	if (xhr && ecmascript_found(xhr->interpreter)) {
		struct ecmascript_interpreter *interpreter = xhr->interpreter;
		js_State *J = (js_State *)interpreter->backend_data;

		struct xhr_listener *l;

		foreach(l, xhr->listeners) {
			if (strcmp(l->typ, "loadend")) {
				continue;
			}
			js_getregistry(J, l->fun);
			js_getregistry(J, xhr->thisval);
			js_pcall(J, 0);
			js_pop(J, 1);
		}

		if (xhr->onloadend) {
			js_getregistry(J, xhr->onloadend); /* retrieve the js function from the registry */
			js_getregistry(J, xhr->thisval);
			js_pcall(J, 0);
			js_pop(J, 1);
		}
		check_for_rerender(interpreter, "xhr_onloadend");
	}
}

static void
onreadystatechange_run(void *data)
{
	struct mjs_xhr *xhr = (struct mjs_xhr *)data;

	if (xhr && ecmascript_found(xhr->interpreter)) {
		struct ecmascript_interpreter *interpreter = xhr->interpreter;
		js_State *J = (js_State *)interpreter->backend_data;

		struct xhr_listener *l;

		foreach(l, xhr->listeners) {
			if (strcmp(l->typ, "readystatechange")) {
				continue;
			}
			js_getregistry(J, l->fun);
			js_getregistry(J, xhr->thisval);
			js_pcall(J, 0);
			js_pop(J, 1);
		}

		if (xhr->onreadystatechange) {
			js_getregistry(J, xhr->onreadystatechange); /* retrieve the js function from the registry */
			js_getregistry(J, xhr->thisval);
			js_pcall(J, 0);
			js_pop(J, 1);
		}
		check_for_rerender(interpreter, "xhr_onreadystatechange");
	}
}

static void
ontimeout_run(void *data)
{
	struct mjs_xhr *xhr = (struct mjs_xhr *)data;

	if (xhr && ecmascript_found(xhr->interpreter)) {
		struct ecmascript_interpreter *interpreter = xhr->interpreter;
		js_State *J = (js_State *)interpreter->backend_data;

		struct xhr_listener *l;

		foreach(l, xhr->listeners) {
			if (strcmp(l->typ, "timeout")) {
				continue;
			}
			js_getregistry(J, l->fun);
			js_getregistry(J, xhr->thisval);
			js_pcall(J, 0);
			js_pop(J, 1);
		}

		if (xhr->ontimeout) {
			js_getregistry(J, xhr->ontimeout); /* retrieve the js function from the registry */
			js_getregistry(J, xhr->thisval);
			js_pcall(J, 0);
			js_pop(J, 1);
		}
		check_for_rerender(interpreter, "xhr_ontimeout");
	}
}

static void
mjs_xhr_loading_callback(struct download *download, struct mjs_xhr *xhr)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	if (is_in_state(download->state, S_TIMEOUT)) {
		if (xhr->readyState != DONE) {
			xhr->readyState = DONE;
			register_bottom_half(onreadystatechange_run, xhr);
		}
		register_bottom_half(ontimeout_run, xhr);
		register_bottom_half(onloadend_run, xhr);
	} else if (is_in_result_state(download->state)) {
		struct cache_entry *cached = download->cached;

		if (!cached) {
			return;
		}
		struct fragment *fragment = get_cache_fragment(cached);

		if (!fragment) {
			return;
		}
		if (cached->head) {
			process_xhr_headers(cached->head, xhr);
		}
		mem_free_set(&xhr->responseText, memacpy(fragment->data, fragment->length));
		mem_free_set(&xhr->responseType, stracpy(""));
		if (xhr->readyState != DONE) {
			xhr->readyState = DONE;
			register_bottom_half(onreadystatechange_run, xhr);
		}
		register_bottom_half(onload_run, xhr);
		register_bottom_half(onloadend_run, xhr);
	}
}

static size_t
write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct mjs_xhr *xhr = (struct mjs_xhr *)stream;

	size_t length = xhr->responseLength;

	char *n = (char *)mem_realloc(xhr->responseText, length + size * nmemb + 1);

	if (n) {
		xhr->responseText = n;
	} else {
		return 0;
	}
	memcpy(xhr->responseText + length, ptr, (size * nmemb));
	xhr->responseText[length + size * nmemb] = '\0';
	xhr->responseLength += size * nmemb;

	return nmemb;
}

static void
mjs_xhr_send(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");
	struct view_state *vs;
	struct document_view *doc_view;
	vs = interpreter->vs;
	doc_view = vs->doc_view;

	if (!xhr) {
		js_pushnull(J);
		return;
	}

	if (xhr->readyState != OPENED) {
		js_pushnull(J);
		return;
	}

	if (xhr->isSend) {
		js_pushnull(J);
		return;
	}

	const char *body = NULL;

	if (xhr->async && xhr->method == POST && !js_isundefined(J, 1)) {
		body = js_tostring(J, 1);

		if (body) {
			struct string post;
			if (!init_string(&post)) {
				js_error(J, "out of memory");
				return;
			}

			add_to_string(&post, "text/plain\n");
			for (int i = 0; body[i]; i++) {
				char p[3];

				ulonghexcat(p, NULL, (int)body[i], 2, '0', 0);
				add_to_string(&post, p);
			}
			xhr->uri->post = post.source;
			char *url2 = get_uri_string(xhr->uri, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD | URI_POST);
			done_string(&post);

			if (!url2) {
				js_error(J, "!url2");
				return;
			}
			done_uri(xhr->uri);
			xhr->uri = get_uri(url2, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD | URI_POST);
			mem_free(url2);
		}
	}

	if (xhr->uri) {
		if (xhr->uri->protocol == PROTOCOL_FILE && !get_opt_bool("ecmascript.allow_xhr_file", NULL)) {
			js_pushundefined(J);
			return;
		}

		if (!xhr->async) {
#ifdef CONFIG_LIBCURL
			char *url = get_uri_string(xhr->uri, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD);

			if (!url) {
				js_pushundefined(J);
				return;
			}
			xhr->isSend = true;
			CURL *curl_handle = curl_easy_init();
			curl_easy_setopt(curl_handle, CURLOPT_URL, url);
			curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
			curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
			curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
			curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, xhr);

			if (!js_isundefined(J, 1)) {
				const char *body = js_tostring(J, 1);
				size_t size = strlen(body);

				if (body) {
					curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long) size);
					curl_easy_setopt(curl_handle, CURLOPT_COPYPOSTFIELDS, body);
				}
			}

			curl_easy_perform(curl_handle);
			curl_easy_cleanup(curl_handle);
			xhr->readyState = DONE;
			xhr->status = 200;
			mem_free(url);

			js_pushundefined(J);
			return;
#else
			js_error(J, "unimplemented");
			return;
#endif
		}
		xhr->download.data = xhr;
		xhr->download.callback = (download_callback_T *)mjs_xhr_loading_callback;
		load_uri(xhr->uri, doc_view->session->referrer, &xhr->download, PRI_MAIN, CACHE_MODE_NORMAL, -1);
		if (xhr->timeout) {
			set_connection_timeout_xhr(xhr->download.conn, xhr->timeout);
		}
	}
	js_pushundefined(J);
}

static bool
valid_header(const char *header)
{
	if (!*header) {
		return false;
	}

	for (const char *c = header; *c; c++) {
		if (*c < 33 || *c > 127) {
			return false;
		}
	}
	return (NULL == strpbrk(header, "()<>@,;:\\\"/[]?={}"));
}

static bool
forbidden_header(const char *header)
{
	const char *bad[] = {
		"Accept-Charset"
		"Accept-Encoding",
		"Access-Control-Request-Headers",
		"Access-Control-Request-Method",
		"Connection",
		"Content-Length",
		"Cookie",
		"Cookie2",
		"Date",
		"DNT",
		"Expect",
		"Host",
		"Keep-Alive",
		"Origin",
		"Referer",
		"Set-Cookie",
		"TE",
		"Trailer",
		"Transfer-Encoding",
		"Upgrade",
		"Via",
		NULL
	};

	for (int i = 0; bad[i]; i++) {
		if (!strcasecmp(header, bad[i])) {
			return true;
		}
	}

	if (!strncasecmp(header, "proxy-", 6)) {
		return true;
	}

	if (!strncasecmp(header, "sec-", 4)) {
		return true;
	}

	return false;
}

static void
mjs_xhr_setRequestHeader(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}

	if (xhr->readyState != OPENED || xhr->isSend) {
		js_pushnull(J);
		return;
	}
	const char *header = js_isundefined(J, 1) ? NULL : js_tostring(J, 1);

	if (!header) {
		js_pushnull(J);
		return;
	}
	char *value = js_isundefined(J, 2) ? NULL : null_or_stracpy(js_tostring(J, 2));

	if (value) {
		char *normalized_value = normalize(value);

		if (!valid_header(header)) {
			mem_free(value);
			js_pushnull(J);
			return;
		}

		if (forbidden_header(header)) {
			mem_free(value);
			js_pushundefined(J);
			return;
		}
		set_xhr_header(normalized_value, header, xhr);
		mem_free(value);
	}
	js_pushundefined(J);
}

static void
mjs_xhr_get_property_onabort(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, xhr->onabort);
}

static void
mjs_xhr_set_property_onabort(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	if (xhr->onabort) {
		js_unref(J, xhr->onabort);
	}
	js_copy(J, 1);
	xhr->onabort = js_ref(J);

	js_pushundefined(J);
}

static void
mjs_xhr_get_property_onerror(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, xhr->onerror);
}

static void
mjs_xhr_set_property_onerror(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	if (xhr->onerror) {
		js_unref(J, xhr->onerror);
	}
	js_copy(J, 1);
	xhr->onerror = js_ref(J);

	js_pushundefined(J);
}

static void
mjs_xhr_get_property_onload(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, xhr->onload);
}

static void
mjs_xhr_set_property_onload(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	if (xhr->onload) {
		js_unref(J, xhr->onload);
	}
	js_copy(J, 1);
	xhr->onload = js_ref(J);

	js_pushundefined(J);
}

static void
mjs_xhr_get_property_onloadend(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, xhr->onloadend);
}

static void
mjs_xhr_set_property_onloadend(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	if (xhr->onloadend) {
		js_unref(J, xhr->onloadend);
	}
	js_copy(J, 1);
	xhr->onloadend = js_ref(J);

	js_pushundefined(J);
}

static void
mjs_xhr_get_property_onloadstart(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, xhr->onloadstart);
}

static void
mjs_xhr_set_property_onloadstart(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	if (xhr->onloadstart) {
		js_unref(J, xhr->onloadstart);
	}
	js_copy(J, 1);
	xhr->onloadstart = js_ref(J);

	js_pushundefined(J);
}

static void
mjs_xhr_get_property_onprogress(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, xhr->onprogress);
}

static void
mjs_xhr_set_property_onprogress(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	if (xhr->onprogress) {
		js_unref(J, xhr->onprogress);
	}
	js_copy(J, 1);
	xhr->onprogress = js_ref(J);

	js_pushundefined(J);
}

static void
mjs_xhr_get_property_onreadystatechange(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, xhr->onreadystatechange);
}

static void
mjs_xhr_set_property_onreadystatechange(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	if (xhr->onreadystatechange) {
		js_unref(J, xhr->onreadystatechange);
	}
	js_copy(J, 1);
	xhr->onreadystatechange = js_ref(J);

	js_pushundefined(J);
}

static void
mjs_xhr_get_property_ontimeout(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_getregistry(J, xhr->ontimeout);
}

static void
mjs_xhr_set_property_ontimeout(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	if (xhr->ontimeout) {
		js_unref(J, xhr->ontimeout);
	}
	js_copy(J, 1);
	xhr->ontimeout = js_ref(J);

	js_pushundefined(J);
}

static void
mjs_xhr_get_property_readyState(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, xhr->readyState);
}

static void
mjs_xhr_get_property_response(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr || !xhr->response) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, xhr->response);
}

static void
mjs_xhr_get_property_responseText(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}

	if (xhr->readyState != LOADING && xhr->readyState != DONE) {
		js_pushstring(J, "");
		return;
	}
	js_pushstring(J, xhr->responseText);
}

static void
mjs_xhr_get_property_responseType(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr || !xhr->responseType) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, xhr->responseType);
}

static void
mjs_xhr_get_property_responseURL(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr || !xhr->responseURL) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, xhr->responseURL);
}

static void
mjs_xhr_get_property_status(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, xhr->status);
}

static void
mjs_xhr_get_property_statusText(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr || !xhr->statusText) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, xhr->statusText);
}

static void
mjs_xhr_get_property_upload(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr || !xhr->upload) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, xhr->upload);
}

static void
mjs_xhr_get_property_timeout(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, xhr->timeout);
}

static void
mjs_xhr_get_property_withCredentials(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	js_pushboolean(J, xhr->withCredentials);
}

static void
mjs_xhr_set_property_responseType(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}
	mem_free_set(&xhr->responseType, null_or_stracpy(js_tostring(J, 1)));

	js_pushundefined(J);
}

static void
mjs_xhr_set_property_timeout(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr || !xhr->async) {
		js_pushnull(J);
		return;
	}
	xhr->timeout = js_toint32(J, 1);

	js_pushundefined(J);
}

static void
mjs_xhr_set_property_withCredentials(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_xhr *xhr = (struct mjs_xhr *)js_touserdata(J, 0, "xhr");

	if (!xhr) {
		js_pushnull(J);
		return;
	}

	if (xhr->readyState != UNSENT && xhr->readyState != OPENED) {
		js_pushnull(J);
		return;
	}

	if (xhr->isSend) {
		js_pushnull(J);
		return;
	}
	xhr->withCredentials = js_toboolean(J, 1);
	/// TODO Set this’s cross-origin credentials to the given value.

	js_pushundefined(J);
}

static void
mjs_xhr_fun(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
mjs_xhr_constructor(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	struct mjs_xhr *xhr = (struct mjs_xhr *)mem_calloc(1, sizeof(*xhr));

	if (!xhr) {
		return;
	}
	xhr->interpreter = interpreter;
	xhr->async = true;

	xhr->requestHeaders = attr_create_new_requestHeaders_map();
	xhr->responseHeaders = attr_create_new_responseHeaders_map();

	init_list(xhr->listeners);

	js_newobject(J);
	{
		xhr->thisval = js_ref(J);
		js_newuserdata(J, "xhr", xhr, mjs_xhr_finalizer);
		addmethod(J, "XMLHttpRequest.prototype.abort", mjs_xhr_abort, 0);
		addmethod(J, "XMLHttpRequest.prototype.addEventListener", mjs_xhr_addEventListener, 3);
		addmethod(J, "XMLHttpRequest.prototype.getAllResponseHeaders", mjs_xhr_getAllResponseHeaders, 0);
		addmethod(J, "XMLHttpRequest.prototype.getResponseHeader", mjs_xhr_getResponseHeader, 1);
		addmethod(J, "XMLHttpRequest.prototype.open", mjs_xhr_open, 5);
		addmethod(J, "XMLHttpRequest.prototype.overrideMimeType", mjs_xhr_overrideMimeType, 1);
		addmethod(J, "XMLHttpRequest.prototype.removeEventListener", mjs_xhr_removeEventListener, 3);
		addmethod(J, "XMLHttpRequest.prototype.send", mjs_xhr_send, 1);
		addmethod(J, "XMLHttpRequest.prototype.setRequestHeader", mjs_xhr_setRequestHeader, 2);

		addproperty(J, "UNSENT", mjs_xhr_static_get_property_UNSENT, NULL);
		addproperty(J, "OPENED", mjs_xhr_static_get_property_OPENED, NULL);
		addproperty(J, "HEADERS_RECEIVED", mjs_xhr_static_get_property_HEADERS_RECEIVED, NULL);
		addproperty(J, "LOADING", mjs_xhr_static_get_property_LOADING, NULL);
		addproperty(J, "DONE", mjs_xhr_static_get_property_DONE, NULL);

		addproperty(J, "onabort", mjs_xhr_get_property_onabort, mjs_xhr_set_property_onabort);
		addproperty(J, "onerror", mjs_xhr_get_property_onerror, mjs_xhr_set_property_onerror);
		addproperty(J, "onload", mjs_xhr_get_property_onload, mjs_xhr_set_property_onload);
		addproperty(J, "onloadend", mjs_xhr_get_property_onloadend, mjs_xhr_set_property_onloadend);
		addproperty(J, "onloadstart", mjs_xhr_get_property_onloadstart, mjs_xhr_set_property_onloadstart);
		addproperty(J, "onprogress",	mjs_xhr_get_property_onprogress, mjs_xhr_set_property_onprogress);
		addproperty(J, "onreadystatechange", mjs_xhr_get_property_onreadystatechange, mjs_xhr_set_property_onreadystatechange);
		addproperty(J, "ontimeout",	mjs_xhr_get_property_ontimeout, mjs_xhr_set_property_ontimeout);
		addproperty(J, "readyState",	mjs_xhr_get_property_readyState, NULL);
		addproperty(J, "response",	mjs_xhr_get_property_response, NULL);
		addproperty(J, "responseText",	mjs_xhr_get_property_responseText, NULL);
		addproperty(J, "responseType",	mjs_xhr_get_property_responseType, mjs_xhr_set_property_responseType);
		addproperty(J, "responseURL",	mjs_xhr_get_property_responseURL, NULL);
		addproperty(J, "status",	mjs_xhr_get_property_status, NULL);
		addproperty(J, "statusText",	mjs_xhr_get_property_statusText, NULL);
		addproperty(J, "timeout",	mjs_xhr_get_property_timeout, mjs_xhr_set_property_timeout);
		addproperty(J, "upload",	mjs_xhr_get_property_upload, NULL);
		addproperty(J, "withCredentials",mjs_xhr_get_property_withCredentials, mjs_xhr_set_property_withCredentials);
	}
}

int
mjs_xhr_init(js_State *J)
{
	js_pushglobal(J);
	js_newcconstructor(J, mjs_xhr_fun, mjs_xhr_constructor, "XMLHttpRequest", 0);
	js_defglobal(J, "XMLHttpRequest", JS_DONTENUM);
	return 0;
}
