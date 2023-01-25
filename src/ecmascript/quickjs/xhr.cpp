/*
 * QuickJS libuv bindings
 *
 * Copyright (c) 2019-present Saúl Ibarra Corretgé <s@saghul.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* The QuickJS xhr object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

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
#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/heartbeat.h"
#include "ecmascript/quickjs/xhr.h"
#include "ecmascript/timer.h"
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

#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

enum {
    XHR_EVENT_ABORT = 0,
    XHR_EVENT_ERROR,
    XHR_EVENT_LOAD,
    XHR_EVENT_LOAD_END,
    XHR_EVENT_LOAD_START,
    XHR_EVENT_PROGRESS,
    XHR_EVENT_READY_STATE_CHANGED,
    XHR_EVENT_TIMEOUT,
    XHR_EVENT_MAX,
};

enum {
    XHR_RSTATE_UNSENT = 0,
    XHR_RSTATE_OPENED,
    XHR_RSTATE_HEADERS_RECEIVED,
    XHR_RSTATE_LOADING,
    XHR_RSTATE_DONE,
};

enum {
    XHR_RTYPE_DEFAULT = 0,
    XHR_RTYPE_TEXT,
    XHR_RTYPE_ARRAY_BUFFER,
    XHR_RTYPE_JSON,
};

enum {
    GET = 1,
    HEAD = 2,
    POST = 3
};

struct classcomp {
	bool operator() (const std::string& lhs, const std::string& rhs) const
	{
		return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};

static char *normalize(char *value);

static char *
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

static bool
valid_header(char *header)
{
	if (!*header) {
		return false;
	}

	for (char *c = header; *c; c++) {
		if (*c < 33 || *c > 127) {
			return false;
		}
	}
	return (NULL == strpbrk(header, "()<>@,;:\\\"/[]?={}"));
}

static bool
forbidden_header(char *header)
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

struct listener {
	LIST_HEAD(struct listener);
	char *typ;
	JSValue fun;
};

typedef struct {
	std::map<std::string, std::string> requestHeaders;
	std::map<std::string, std::string, classcomp> responseHeaders;
	struct download download;
	struct ecmascript_interpreter *interpreter;

	LIST_OF(struct listener) listeners;

	JSValue events[XHR_EVENT_MAX];
	JSValue thisVal;
	struct uri *uri;
	bool sent;
	bool async;
	short response_type;
	unsigned long timeout;
	unsigned short ready_state;

	int status;
	char *status_text;
	char *response_text;

	struct {
		JSValue url;
		JSValue headers;
		JSValue response;
	} result;

	char *responseURL;
	int method;
} Xhr;

static JSClassID xhr_class_id;

static void onload_run(void *data);
static void onloadend_run(void *data);
static void onreadystatechange_run(void *data);
static void ontimeout_run(void *data);

static void
onload_run(void *data)
{
	Xhr *x = (Xhr *)data;

	if (x) {
		struct ecmascript_interpreter *interpreter = x->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct listener *l;

		foreach(l, x->listeners) {
			if (strcmp(l->typ, "load")) {
				continue;
			}
			JSValue func = JS_DupValue(ctx, l->fun);
			JSValue arg = JS_UNDEFINED;
			JSValue ret = JS_Call(ctx, func, x->thisVal, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		JSValue event_func = x->events[XHR_EVENT_LOAD];

		if (JS_IsFunction(ctx, event_func)) {
			JSValue func = JS_DupValue(ctx, event_func);
			JSValue arg = JS_UNDEFINED;
			JSValue ret = JS_Call(ctx, func, x->thisVal, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		done_heartbeat(interpreter->heartbeat);
		check_for_rerender(interpreter, "xhr_onload");
	}
}

static void
onloadend_run(void *data)
{
	Xhr *x = (Xhr *)data;

	if (x) {
		struct ecmascript_interpreter *interpreter = x->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct listener *l;

		foreach(l, x->listeners) {
			if (strcmp(l->typ, "loadend")) {
				continue;
			}
			JSValue func = JS_DupValue(ctx, l->fun);
			JSValue arg = JS_UNDEFINED;
			JSValue ret = JS_Call(ctx, func, x->thisVal, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		JSValue event_func = x->events[XHR_EVENT_LOAD_END];

		if (JS_IsFunction(ctx, event_func)) {
			JSValue func = JS_DupValue(ctx, event_func);
			JSValue arg = JS_UNDEFINED;
			JSValue ret = JS_Call(ctx, func, x->thisVal, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		done_heartbeat(interpreter->heartbeat);
		check_for_rerender(interpreter, "xhr_onloadend");
	}
}

static void
onreadystatechange_run(void *data)
{
	Xhr *x = (Xhr *)data;

	if (x) {
		struct ecmascript_interpreter *interpreter = x->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct listener *l;

		foreach(l, x->listeners) {
			if (strcmp(l->typ, "readystatechange")) {
				continue;
			}
			JSValue func = JS_DupValue(ctx, l->fun);
			JSValue arg = JS_UNDEFINED;
			JSValue ret = JS_Call(ctx, func, x->thisVal, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		JSValue event_func = x->events[XHR_EVENT_READY_STATE_CHANGED];

		if (JS_IsFunction(ctx, event_func)) {
			JSValue func = JS_DupValue(ctx, event_func);
			JSValue arg = JS_UNDEFINED;
			JSValue ret = JS_Call(ctx, func, x->thisVal, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		done_heartbeat(interpreter->heartbeat);
		check_for_rerender(interpreter, "xhr_onreadystatechange");
	}
}

static void
ontimeout_run(void *data)
{
	Xhr *x = (Xhr *)data;

	if (x) {
		struct ecmascript_interpreter *interpreter = x->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct listener *l;

		foreach(l, x->listeners) {
			if (strcmp(l->typ, "timeout")) {
				continue;
			}
			JSValue func = JS_DupValue(ctx, l->fun);
			JSValue arg = JS_UNDEFINED;
			JSValue ret = JS_Call(ctx, func, x->thisVal, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		JSValue event_func = x->events[XHR_EVENT_TIMEOUT];

		if (JS_IsFunction(ctx, event_func)) {
			JSValue func = JS_DupValue(ctx, event_func);
			JSValue arg = JS_UNDEFINED;
			JSValue ret = JS_Call(ctx, func, x->thisVal, 1, (JSValueConst *) &arg);
			JS_FreeValue(ctx, ret);
			JS_FreeValue(ctx, func);
			JS_FreeValue(ctx, arg);
		}
		done_heartbeat(interpreter->heartbeat);
		check_for_rerender(interpreter, "xhr_ontimeout");
	}
}

static void
xhr_finalizer(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	Xhr *x = (Xhr *)JS_GetOpaque(val, xhr_class_id);

	if (x) {
		for (int i = 0; i < XHR_EVENT_MAX; i++) {
			JS_FreeValueRT(rt, x->events[i]);
		}
		JS_FreeValueRT(rt, x->result.url);
		JS_FreeValueRT(rt, x->result.headers);
		JS_FreeValueRT(rt, x->result.response);

		if (x->uri) {
			done_uri(x->uri);
		}
		mem_free_if(x->responseURL);
		mem_free_if(x->status_text);
		x->responseHeaders.clear();
		x->requestHeaders.clear();

		struct listener *l;

		foreach(l, x->listeners) {
			mem_free_set(&l->typ, NULL);
			JS_FreeValueRT(rt, l->fun);
		}
		free_list(x->listeners);

		mem_free(x);
	}
}

static void
xhr_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	Xhr *x = (Xhr *)JS_GetOpaque(val, xhr_class_id);

	if (x) {
		for (int i = 0; i < XHR_EVENT_MAX; i++) {
			JS_MarkValue(rt, x->events[i], mark_func);
		}
		JS_MarkValue(rt, x->thisVal, mark_func);
		JS_MarkValue(rt, x->result.url, mark_func);
		JS_MarkValue(rt, x->result.headers, mark_func);
		JS_MarkValue(rt, x->result.response, mark_func);

		struct listener *l;

		foreach(l, x->listeners) {
			JS_MarkValue(rt, l->fun, mark_func);
		}
	}
}

static JSClassDef xhr_class = {
	"XMLHttpRequest",
	.finalizer = xhr_finalizer,
	.gc_mark = xhr_mark,
};

static Xhr *
xhr_get(JSContext *ctx, JSValueConst obj)
{
	REF_JS(obj);

	return (Xhr *)JS_GetOpaque2(ctx, obj, xhr_class_id);
}

static const std::vector<std::string>
explode(const std::string& s, const char& c)
{
	std::string buff{""};
	std::vector<std::string> v;

	bool found = false;
	for (auto n:s) {
		if (found) {
			buff += n;
			continue;
		}
		if (n != c) {
			buff += n;
		}
		else if (n == c && buff != "") {
			v.push_back(buff);
			buff = "";
			found = true;
		}
	}

	if (buff != "") {
		v.push_back(buff);
	}

	return v;
}

static void
x_loading_callback(struct download *download, Xhr *x)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	if (is_in_state(download->state, S_TIMEOUT)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s S_TIMEOUT\n", __FILE__, __FUNCTION__);
#endif
		if (x->ready_state != XHR_RSTATE_DONE) {
			x->ready_state = XHR_RSTATE_DONE;
			register_bottom_half(onreadystatechange_run, x);
		}
		register_bottom_half(ontimeout_run, x);
		register_bottom_half(onloadend_run, x);
	} else if (is_in_result_state(download->state)) {
		struct cache_entry *cached = download->cached;
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s is_in_result_state\n", __FILE__, __FUNCTION__);
#endif
		if (!cached) {
			return;
		}
		struct fragment *fragment = get_cache_fragment(cached);

		if (!fragment) {
			return;
		}
		if (cached->head) {
			std::istringstream headers(cached->head);

			std::string http;
			int status;
			std::string statusText;

			std::string line;

			std::getline(headers, line);

			std::istringstream linestream(line);
			linestream >> http >> status >> statusText;

			while (1) {
				std::getline(headers, line);
				if (line.empty()) {
					break;
				}
				std::vector<std::string> v = explode(line, ':');
				if (v.size() == 2) {
					char *value = stracpy(v[1].c_str());

					if (!value) {
						continue;
					}
					char *header = stracpy(v[0].c_str());
					if (!header) {
						mem_free(value);
						continue;
					}
					char *normalized_value = normalize(value);
					bool found = false;

					for (auto h: x->responseHeaders) {
						const std::string hh = h.first;
						if (!strcasecmp(hh.c_str(), header)) {
							x->responseHeaders[hh] = x->responseHeaders[hh] + ", " + normalized_value;
							found = true;
							break;
						}
					}

					if (!found) {
						x->responseHeaders[header] = normalized_value;
					}
					mem_free(header);
					mem_free(value);
				}
			}
			x->status = status;
			mem_free_set(&x->status_text, stracpy(statusText.c_str()));
		}
		mem_free_set(&x->response_text, memacpy(fragment->data, fragment->length));
		if (x->ready_state != XHR_RSTATE_DONE) {
			x->ready_state = XHR_RSTATE_DONE;
			register_bottom_half(onreadystatechange_run, x);
		}
		register_bottom_half(onloadend_run, x);
		register_bottom_half(onload_run, x);
	} else {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s else\n", __FILE__, __FUNCTION__);
#endif
	}
}

static JSValue
xhr_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(new_target);

	JSValue obj = JS_NewObjectClass(ctx, xhr_class_id);
	REF_JS(obj);

	if (JS_IsException(obj)) {
		return obj;
	}
	Xhr *x = (Xhr *)mem_calloc(1, sizeof(*x));

	if (!x) {
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

	x->interpreter = interpreter;
	x->result.url = JS_NULL;
	x->result.headers = JS_NULL;
	x->result.response = JS_NULL;
//    dbuf_init(&x->result.hbuf);
//    dbuf_init(&x->result.bbuf);
	x->ready_state = XHR_RSTATE_UNSENT;
//    x->slist = NULL;
	x->sent = false;
	x->async = true;

	init_list(x->listeners);

	for (int i = 0; i < XHR_EVENT_MAX; i++) {
		x->events[i] = JS_UNDEFINED;
	}
	JS_SetOpaque(obj, x);
	x->thisVal = JS_DupValue(ctx, obj);

	return obj;
}

static JSValue
xhr_event_get(JSContext *ctx, JSValueConst this_val, int magic)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_DupValue(ctx, x->events[magic]);
}

static JSValue
xhr_event_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(value);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	if (JS_IsFunction(ctx, value) || JS_IsUndefined(value) || JS_IsNull(value)) {
		JS_FreeValue(ctx, x->events[magic]);
		x->events[magic] = JS_DupValue(ctx, value);
	}
	return JS_UNDEFINED;
}

static JSValue
xhr_readystate_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_NewInt32(ctx, x->ready_state);
}

static JSValue
xhr_response_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}
// TODO
	return JS_NULL;
#if 0
//    DynBuf *bbuf = &x->result.bbuf;
    if (bbuf->size == 0)
        return JS_NULL;
    if (JS_IsNull(x->result.response)) {
        switch (x->response_type) {
            case XHR_RTYPE_DEFAULT:
            case XHR_RTYPE_TEXT:
                x->result.response = JS_NewStringLen(ctx, (char *) bbuf->buf, bbuf->size);
                break;
            case XHR_RTYPE_ARRAY_BUFFER:
                x->result.response = JS_NewArrayBufferCopy(ctx, bbuf->buf, bbuf->size);
                break;
            case XHR_RTYPE_JSON:
                // It's necessary to null-terminate the string passed to JS_ParseJSON.
                dbuf_putc(bbuf, '\0');
                x->result.response = JS_ParseJSON(ctx, (char *) bbuf->buf, bbuf->size, "<xhr>");
                break;
            default:
                abort();
        }
    }
    return JS_DupValue(ctx, x->result.response);
#endif
}

static JSValue
xhr_responsetext_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

//DynBuf *bbuf = &x->result.bbuf;
//    if (bbuf->size == 0)
//        return JS_NULL;
	if (!x->response_text) {
		return JS_NULL;
	}

	return JS_NewString(ctx, x->response_text);
}

static JSValue
xhr_responsetype_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	switch (x->response_type) {
	case XHR_RTYPE_DEFAULT:
		return JS_NewString(ctx, "");
	case XHR_RTYPE_TEXT:
		return JS_NewString(ctx, "text");
	case XHR_RTYPE_ARRAY_BUFFER:
		return JS_NewString(ctx, "arraybuffer");
	case XHR_RTYPE_JSON:
		return JS_NewString(ctx, "json");
	default:
		return JS_NULL;//abort();
	}
}

static JSValue
xhr_responsetype_set(JSContext *ctx, JSValueConst this_val, JSValueConst value)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(value);

	static const char array_buffer[] = "arraybuffer";
	static const char json[] = "json";
	static const char text[] = "text";

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	if (x->ready_state >= XHR_RSTATE_LOADING) {
		JS_Throw(ctx, JS_NewString(ctx, "InvalidStateError"));
	}

	const char *v = JS_ToCString(ctx, value);

	if (v) {
		if (strncmp(array_buffer, v, sizeof(array_buffer) - 1) == 0) {
			x->response_type = XHR_RTYPE_ARRAY_BUFFER;
		} else if (strncmp(json, v, sizeof(json) - 1) == 0) {
			x->response_type = XHR_RTYPE_JSON;
		} else if (strncmp(text, v, sizeof(text) - 1) == 0) {
			x->response_type = XHR_RTYPE_TEXT;
		} else if (strlen(v) == 0) {
			x->response_type = XHR_RTYPE_DEFAULT;
		}
		JS_FreeCString(ctx, v);
	}

	return JS_UNDEFINED;
}

static JSValue
xhr_responseurl_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_DupValue(ctx, x->result.url);
}

static JSValue
xhr_status_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_NewInt32(ctx, x->status);
}

static JSValue
xhr_statustext_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_NewString(ctx, x->status_text);
}

static JSValue
xhr_timeout_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_NewInt32(ctx, x->timeout);
}

static JSValue
xhr_timeout_set(JSContext *ctx, JSValueConst this_val, JSValueConst value)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(value);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x || !x->async) {
		return JS_EXCEPTION;
	}
	int32_t timeout;

	if (JS_ToInt32(ctx, &timeout, value)) {
		return JS_EXCEPTION;
	}

	x->timeout = timeout;

//    if (!x->sent)
//        curl_easy_setopt(x->curl_h, CURLOPT_TIMEOUT_MS, timeout);

	return JS_UNDEFINED;
}

static JSValue
xhr_upload_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	// TODO.
	return JS_UNDEFINED;
}

static JSValue
xhr_withcredentials_get(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	// TODO.
	return JS_UNDEFINED;
}

static JSValue
xhr_withcredentials_set(JSContext *ctx, JSValueConst this_val, JSValueConst value)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(value);
	// TODO.
	return JS_UNDEFINED;
}

static JSValue
xhr_abort(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}
	x->ready_state = XHR_RSTATE_UNSENT;
	x->status = 0;
	mem_free_set(&x->status_text, NULL);

	if (x->download.conn) {
		abort_connection(x->download.conn, connection_state(S_INTERRUPTED));
	}
	//maybe_emit_event(x, XHR_EVENT_ABORT, JS_UNDEFINED);
	return JS_UNDEFINED;
}

static JSValue
xhr_addEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	if (argc < 2) {
		return JS_UNDEFINED;
	}

	const char *str = JS_ToCString(ctx, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}

	char *method = stracpy(str);
	JS_FreeCString(ctx, str);

	if (!method) {
		return JS_EXCEPTION;
	}
	JSValueConst fun = argv[1];
	struct listener *l;

	foreach(l, x->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			mem_free(method);
			return JS_UNDEFINED;
		}
	}
	struct listener *n = (struct listener *)mem_calloc(1, sizeof(*n));

	if (n) {
		n->typ = method;
		n->fun = JS_DupValue(ctx, fun);
		add_to_list_end(x->listeners, n);
	}
	return JS_UNDEFINED;
}

static JSValue
xhr_removeEventListener(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	if (argc < 2) {
		return JS_UNDEFINED;
	}
	const char *str = JS_ToCString(ctx, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}

	char *method = stracpy(str);
	JS_FreeCString(ctx, str);

	if (!method) {
		return JS_EXCEPTION;
	}
	JSValueConst fun = argv[1];
	struct listener *l;

	foreach(l, x->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			JS_FreeValue(ctx, l->fun);
			mem_free(l);
			mem_free(method);

			return JS_UNDEFINED;
		}
	}
	mem_free(method);

	return JS_UNDEFINED;
}

static JSValue
xhr_getallresponseheaders(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}
	std::string output = "";

	for (auto h: x->responseHeaders) {
		output += h.first + ": " + h.second + "\r\n";
	}
	return JS_NewString(ctx, output.c_str());
}

static JSValue
xhr_getresponseheader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}
	const char *header_name = JS_ToCString(ctx, argv[0]);

	if (header_name) {
		std::string output = "";
		for (auto h: x->responseHeaders) {
			if (!strcasecmp(header_name, h.first.c_str())) {
				output = h.second;
				break;
			}
		}
		JS_FreeCString(ctx, header_name);
		if (!output.empty()) {
			return JS_NewString(ctx, output.c_str());
		}
	}
	return JS_NULL;
}

static JSValue
xhr_open(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	static const char head_method[] = "HEAD";

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}
	// TODO: support username and password.

	if (x->ready_state == XHR_RSTATE_DONE) {
//        if (x->slist)
//            curl_slist_free_all(x->slist);
		for (int i = 0; i < XHR_EVENT_MAX; i++) {
			JS_FreeValue(ctx, x->events[i]);
		}
		x->status = 0;
		mem_free_set(&x->status_text, NULL);
		JS_FreeValue(ctx, x->result.url);
		JS_FreeValue(ctx, x->result.headers);
		JS_FreeValue(ctx, x->result.response);
//        dbuf_free(&x->result.hbuf);
//        dbuf_free(&x->result.bbuf);

//        dbuf_init(&x->result.hbuf);
//        dbuf_init(&x->result.bbuf);
		x->result.url = JS_NULL;
		x->result.headers = JS_NULL;
		x->result.response = JS_NULL;
		x->ready_state = XHR_RSTATE_UNSENT;
		//x->slist = NULL;
		x->sent = false;
		x->async = true;

		for (int i = 0; i < XHR_EVENT_MAX; i++) {
			x->events[i] = JS_UNDEFINED;
		}
		mem_free_set(&x->response_text, NULL);
	}

	if (x->ready_state < XHR_RSTATE_OPENED) {
		JSValue method = argv[0];
		JSValue url = argv[1];
		JSValue async = argv[2];
		const char *method_str = JS_ToCString(ctx, method);
		const char *url_str = JS_ToCString(ctx, url);

		if (argc == 3) {
			x->async = JS_ToBool(ctx, async);
		}
//        if (strncasecmp(head_method, method_str, sizeof(head_method) - 1) == 0)
//            curl_easy_setopt(x->curl_h, CURLOPT_NOBODY, 1L);
//        else
//            curl_easy_setopt(x->curl_h, CURLOPT_CUSTOMREQUEST, method_str);
//        curl_easy_setopt(x->curl_h, CURLOPT_URL, url_str);

		const char *allowed[] = { "", "GET", "HEAD", "POST", NULL };
		bool method_ok = false;

		for (int i = 1; allowed[i]; i++) {
			if (!strcasecmp(allowed[i], method_str)) {
				method_ok = true;
				x->method = i;
				break;
			}
		}
		mem_free_set(&x->responseURL, stracpy(url_str));
		JS_FreeCString(ctx, method_str);
		JS_FreeCString(ctx, url_str);

		if (!method_ok || !x->responseURL) {
			return JS_UNDEFINED;
		}

		struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
		struct view_state *vs = interpreter->vs;

		if (!strchr(x->responseURL, '/')) {
			char *ref = get_uri_string(vs->uri, URI_DIR_LOCATION | URI_PATH);

			if (ref) {
				char *slash = strrchr(ref, '/');

				if (slash) {
					*slash = '\0';
				}
				char *url = straconcat(ref, "/", x->responseURL, NULL);

				if (url) {
					x->uri = get_uri(url, URI_NONE);
					mem_free(url);
				}
				mem_free(ref);
			}
		}

		if (!x->uri) {
			x->uri = get_uri(x->responseURL, URI_NONE);
		}

		if (!x->uri) {
			return JS_UNDEFINED;
		}

		char *username = NULL;
		char *password = NULL;

		if (argc > 3) {
			username = (char *)JS_ToCString(ctx, argv[3]);
		}
		if (argc > 4) {
			password = (char *)JS_ToCString(ctx, argv[4]);
		}
		if (username || password) {
			if (username) {
				x->uri->user = username;
				x->uri->userlen = strlen(username);
			}
			if (password) {
				x->uri->password = password;
				x->uri->passwordlen = strlen(password);
			}
		}
		char *url2 = get_uri_string(x->uri, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD);

		if (username) {
			JS_FreeCString(ctx, username);
		}
		if (password) {
			JS_FreeCString(ctx, password);
		}

		if (!url2) {
			return JS_UNDEFINED;
		}
		done_uri(x->uri);
		x->uri = get_uri(url2, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD);
		mem_free(url2);

		x->requestHeaders.clear();
		x->responseHeaders.clear();

		x->ready_state = XHR_RSTATE_OPENED;
		register_bottom_half(onreadystatechange_run, x);
	}
	return JS_UNDEFINED;
}

static JSValue
xhr_overridemimetype(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_ThrowTypeError(ctx, "unsupported");
}

static JSValue
xhr_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	if (!x->sent) {
		JSValue arg = argv[0];

		if (x->method == POST && JS_IsString(arg)) {
			size_t size;
			const char *body = JS_ToCStringLen(ctx, &size, arg);

			if (body) {
				struct string post;
				if (!init_string(&post)) {
					JS_FreeCString(ctx, body);
					return JS_EXCEPTION;
				}

				add_to_string(&post, "text/plain\n");
				for (int i = 0; body[i]; i++) {
					char p[3];

					ulonghexcat(p, NULL, (int)body[i], 2, '0', 0);
					add_to_string(&post, p);
				}
				x->uri->post = post.source;
				char *url2 = get_uri_string(x->uri, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD | URI_POST);
				done_string(&post);

				if (!url2) {
					JS_FreeCString(ctx, body);
					return JS_EXCEPTION;
				}
				done_uri(x->uri);
				x->uri = get_uri(url2, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD | URI_POST);
				mem_free(url2);
				JS_FreeCString(ctx, body);
			}
		}

		struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
		struct view_state *vs = interpreter->vs;
		struct document_view *doc_view = vs->doc_view;

		if (x->uri) {
			if (x->uri->protocol == PROTOCOL_FILE && !get_opt_bool("ecmascript.allow_xhr_file", NULL)) {
				return JS_UNDEFINED;
			}
			x->sent = true;
			x->download.data = x;
			x->download.callback = (download_callback_T *)x_loading_callback;
			load_uri(x->uri, doc_view->session->referrer, &x->download, PRI_MAIN, CACHE_MODE_NORMAL, -1);
			if (x->timeout) {
				set_connection_timeout_xhr(x->download.conn, x->timeout);
			}
		}
	}

	return JS_UNDEFINED;
}

static JSValue
xhr_setrequestheader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	if (!JS_IsString(argv[0])) {
		return JS_UNDEFINED;
	}
	const char *h_name, *h_value = NULL;
	h_name = JS_ToCString(ctx, argv[0]);

	if (!h_name) {
		return JS_UNDEFINED;
	}

	if (!JS_IsUndefined(argv[1])) {
		h_value = JS_ToCString(ctx, argv[1]);
	}

	if (h_value) {
		char *copy = stracpy(h_value);

		if (!copy) {
			JS_FreeCString(ctx, h_name);
			JS_FreeCString(ctx, h_value);
			return JS_EXCEPTION;
		}
		char *normalized_value = normalize(copy);

		if (!valid_header(h_name)) {
			JS_FreeCString(ctx, h_name);
			JS_FreeCString(ctx, h_value);
			return JS_UNDEFINED;
		}

		if (forbidden_header(h_name)) {
			JS_FreeCString(ctx, h_name);
			JS_FreeCString(ctx, h_value);
			return JS_UNDEFINED;
		}

		bool found = false;
		for (auto h: x->requestHeaders) {
			const std::string hh = h.first;
			if (!strcasecmp(hh.c_str(), h_name)) {
				x->requestHeaders[hh] = x->requestHeaders[hh] + ", " + normalized_value;
				found = true;
				break;
			}
		}

		if (!found) {
			x->requestHeaders[h_name] = normalized_value;
		}
		JS_FreeCString(ctx, h_value);
		mem_free(copy);
	}
	JS_FreeCString(ctx, h_name);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry xhr_class_funcs[] = {
	JS_PROP_INT32_DEF("UNSENT", XHR_RSTATE_UNSENT, JS_PROP_ENUMERABLE),
	JS_PROP_INT32_DEF("OPENED", XHR_RSTATE_OPENED, JS_PROP_ENUMERABLE),
	JS_PROP_INT32_DEF("HEADERS_RECEIVED", XHR_RSTATE_HEADERS_RECEIVED, JS_PROP_ENUMERABLE),
	JS_PROP_INT32_DEF("LOADING", XHR_RSTATE_LOADING, JS_PROP_ENUMERABLE),
	JS_PROP_INT32_DEF("DONE", XHR_RSTATE_DONE, JS_PROP_ENUMERABLE),
};

static const JSCFunctionListEntry xhr_proto_funcs[] = {
	JS_CGETSET_MAGIC_DEF("onabort", xhr_event_get, xhr_event_set, XHR_EVENT_ABORT),
	JS_CGETSET_MAGIC_DEF("onerror", xhr_event_get, xhr_event_set, XHR_EVENT_ERROR),
	JS_CGETSET_MAGIC_DEF("onload", xhr_event_get, xhr_event_set, XHR_EVENT_LOAD),
	JS_CGETSET_MAGIC_DEF("onloadend", xhr_event_get, xhr_event_set, XHR_EVENT_LOAD_END),
	JS_CGETSET_MAGIC_DEF("onloadstart", xhr_event_get, xhr_event_set, XHR_EVENT_LOAD_START),
	JS_CGETSET_MAGIC_DEF("onprogress", xhr_event_get, xhr_event_set, XHR_EVENT_PROGRESS),
	JS_CGETSET_MAGIC_DEF("onreadystatechange", xhr_event_get, xhr_event_set, XHR_EVENT_READY_STATE_CHANGED),
	JS_CGETSET_MAGIC_DEF("ontimeout", xhr_event_get, xhr_event_set, XHR_EVENT_TIMEOUT),
	JS_CGETSET_DEF("readyState", xhr_readystate_get, NULL),
	JS_CGETSET_DEF("response", xhr_response_get, NULL),
	JS_CGETSET_DEF("responseText", xhr_responsetext_get, NULL),
	JS_CGETSET_DEF("responseType", xhr_responsetype_get, xhr_responsetype_set),
	JS_CGETSET_DEF("responseURL", xhr_responseurl_get, NULL),
	JS_CGETSET_DEF("status", xhr_status_get, NULL),
	JS_CGETSET_DEF("statusText", xhr_statustext_get, NULL),
	JS_CGETSET_DEF("timeout", xhr_timeout_get, xhr_timeout_set),
	JS_CGETSET_DEF("upload", xhr_upload_get, NULL),
	JS_CGETSET_DEF("withCredentials", xhr_withcredentials_get, xhr_withcredentials_set),
	JS_CFUNC_DEF("abort", 0, xhr_abort),
	JS_CFUNC_DEF("addEventListener", 3, xhr_addEventListener),
	JS_CFUNC_DEF("getAllResponseHeaders", 0, xhr_getallresponseheaders),
	JS_CFUNC_DEF("getResponseHeader", 1, xhr_getresponseheader),
	JS_CFUNC_DEF("open", 5, xhr_open),
	JS_CFUNC_DEF("overrideMimeType", 1, xhr_overridemimetype),
	JS_CFUNC_DEF("removeEventListener", 3, xhr_removeEventListener),
	JS_CFUNC_DEF("send", 1, xhr_send),
	JS_CFUNC_DEF("setRequestHeader", 2, xhr_setrequestheader),
};

static void
JS_NewGlobalCConstructor2(JSContext *ctx, JSValue func_obj, const char *name, JSValueConst proto)
{
	REF_JS(func_obj);
	REF_JS(proto);

	JSValue global_object = JS_GetGlobalObject(ctx);

	JS_DefinePropertyValueStr(ctx, global_object, name,
		JS_DupValue(ctx, func_obj), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	JS_SetConstructor(ctx, func_obj, proto);
	JS_FreeValue(ctx, func_obj);
	JS_FreeValue(ctx, global_object);
}

static JSValueConst
JS_NewGlobalCConstructor(JSContext *ctx, const char *name, JSCFunction *func, int length, JSValueConst proto)
{
	JSValue func_obj;
	func_obj = JS_NewCFunction2(ctx, func, name, length, JS_CFUNC_constructor_or_func, 0);
	REF_JS(func_obj);
	REF_JS(proto);

	JS_NewGlobalCConstructor2(ctx, func_obj, name, proto);

	return func_obj;
}

int
js_xhr_init(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto, obj;

	/* XHR class */
	JS_NewClassID(&xhr_class_id);
	JS_NewClass(JS_GetRuntime(ctx), xhr_class_id, &xhr_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, xhr_proto_funcs, countof(xhr_proto_funcs));
	JS_SetClassProto(ctx, xhr_class_id, proto);

	/* XHR object */
	obj = JS_NewGlobalCConstructor(ctx, "XMLHttpRequest", xhr_constructor, 1, proto);
	REF_JS(obj);

	JS_SetPropertyFunctionList(ctx, obj, xhr_class_funcs, countof(xhr_class_funcs));

	return 0;
}
