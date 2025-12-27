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

#ifdef CONFIG_LIBCURL
#include <curl/curl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/quickjs/mapa.h"
#include "js/quickjs.h"
#include "js/quickjs/heartbeat.h"
#include "js/quickjs/xhr.h"
#include "main/select.h"
#include "network/connection.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "util/conv.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

enum {
    GET = 1,
    HEAD = 2,
    POST = 3
};

char *
normalize(char *value)
{
	ELOG
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
valid_header(const char *header)
{
	ELOG
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
	ELOG
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

static JSClassID xhr_class_id;

static void onload_run(void *data);
static void onloadend_run(void *data);
static void onreadystatechange_run(void *data);
static void ontimeout_run(void *data);

static void
onload_run(void *data)
{
	ELOG
	struct Xhr *x = (struct Xhr *)data;

	if (x && ecmascript_found(x->interpreter)) {
		struct ecmascript_interpreter *interpreter = x->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct xhr_listener *l;

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
	ELOG
	struct Xhr *x = (struct Xhr *)data;

	if (x && ecmascript_found(x->interpreter)) {
		struct ecmascript_interpreter *interpreter = x->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct xhr_listener *l;

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
	ELOG
	struct Xhr *x = (struct Xhr *)data;

	if (x && ecmascript_found(x->interpreter)) {
		struct ecmascript_interpreter *interpreter = x->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct xhr_listener *l;

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
	ELOG
	struct Xhr *x = (struct Xhr *)data;

	if (x && ecmascript_found(x->interpreter)) {
		struct ecmascript_interpreter *interpreter = x->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct xhr_listener *l;

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct Xhr *x = (struct Xhr *)JS_GetOpaque(val, xhr_class_id);

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
		mem_free_if(x->response_text);

		struct xhr_listener *l;

		foreach(l, x->listeners) {
			mem_free_set(&l->typ, NULL);
			JS_FreeValueRT(rt, l->fun);
		}
		free_list(x->listeners);

		delete_map_str(x->responseHeaders);
		delete_map_str(x->requestHeaders);
		mem_free(x);
	}
}

static void
xhr_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct Xhr *x = (struct Xhr *)JS_GetOpaque(val, xhr_class_id);

	if (x) {
		for (int i = 0; i < XHR_EVENT_MAX; i++) {
			JS_MarkValue(rt, x->events[i], mark_func);
		}
		JS_MarkValue(rt, x->thisVal, mark_func);
		JS_MarkValue(rt, x->result.url, mark_func);
		JS_MarkValue(rt, x->result.headers, mark_func);
		JS_MarkValue(rt, x->result.response, mark_func);

		struct xhr_listener *l;

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

static struct Xhr *
xhr_get(JSContext *ctx, JSValueConst obj)
{
	ELOG
	REF_JS(obj);

	return (struct Xhr *)JS_GetOpaque2(ctx, obj, xhr_class_id);
}

static void
x_loading_callback(struct download *download, struct Xhr *x)
{
	ELOG
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
			process_xhr_headers(cached->head, x);
		} else {
			x->status = 200;
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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(new_target);

	JSValue obj = JS_NewObjectClass(ctx, xhr_class_id);
	REF_JS(obj);

	if (JS_IsException(obj)) {
		return obj;
	}
	struct Xhr *x = (struct Xhr *)mem_calloc(1, sizeof(*x));

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

	x->requestHeaders = attr_create_new_requestHeaders_map();
	x->responseHeaders = attr_create_new_responseHeaders_map();

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_DupValue(ctx, x->events[magic]);
}

static JSValue
xhr_event_set(JSContext *ctx, JSValueConst this_val, JSValueConst value, int magic)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(value);

	struct Xhr *x = xhr_get(ctx, this_val);

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_NewInt32(ctx, x->ready_state);
}

static JSValue
xhr_response_get(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(value);

	static const char array_buffer[] = "arraybuffer";
	static const char json[] = "json";
	static const char text[] = "text";

	struct Xhr *x = xhr_get(ctx, this_val);

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_DupValue(ctx, x->result.url);
}

static JSValue
xhr_status_get(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_NewInt32(ctx, x->status);
}

static JSValue
xhr_statustext_get(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	if (!x->status_text) {
		return JS_NULL;
	}

	return JS_NewString(ctx, x->status_text);
}

static JSValue
xhr_timeout_get(JSContext *ctx, JSValueConst this_val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	return JS_NewInt32(ctx, x->timeout);
}

static JSValue
xhr_timeout_set(JSContext *ctx, JSValueConst this_val, JSValueConst value)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(value);

	struct Xhr *x = xhr_get(ctx, this_val);

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
	ELOG
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
	ELOG
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
	ELOG
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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

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
	struct xhr_listener *l;

	foreach(l, x->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (JS_VALUE_GET_PTR(l->fun) == JS_VALUE_GET_PTR(fun)) {
			mem_free(method);
			return JS_UNDEFINED;
		}
	}
	struct xhr_listener *n = (struct xhr_listener *)mem_calloc(1, sizeof(*n));

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

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
	struct xhr_listener *l;

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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	JSValue ret;

	struct Xhr *x = xhr_get(ctx, this_val);
	char *output;

	if (!x) {
		return JS_EXCEPTION;
	}
	output = get_output_headers(x);

	if (output) {
		ret = JS_NewString(ctx, output);
		mem_free(output);
		return ret;
	}
	return JS_NULL;
}

static JSValue
xhr_getresponseheader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}
	const char *header_name = JS_ToCString(ctx, argv[0]);

	if (header_name) {
		char *output = get_output_header(header_name, x);

		if (output) {
			JSValue ret = JS_NewString(ctx, output);
			mem_free(output);
			return ret;
		}
	}
	return JS_NULL;
}

static JSValue
xhr_open(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

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
		x->response_length = 0;
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

		if (x->responseURL[0] != '/') {
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
		} else {
			char *ref = get_uri_string(vs->uri, URI_SERVER);

			if (ref) {
				char *url = straconcat(ref, x->responseURL, NULL);

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

		if (argc > 3 && !JS_IsUndefined(argv[3])) {
			username = (char *)JS_ToCString(ctx, argv[3]);
		}

		if (argc > 4 && !JS_IsUndefined(argv[4])) {
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

		delete_map_str(x->requestHeaders);
		delete_map_str(x->responseHeaders);
		x->requestHeaders = attr_create_new_requestHeaders_map();
		x->responseHeaders = attr_create_new_responseHeaders_map();

		x->ready_state = XHR_RSTATE_OPENED;
		register_bottom_half(onreadystatechange_run, x);
	}
	return JS_UNDEFINED;
}

static JSValue
xhr_overridemimetype(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_ThrowTypeError(ctx, "unsupported");
}

static size_t
write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	ELOG
	struct Xhr *x = (struct Xhr *)stream;
	size_t length = x->response_length;

	char *n = (char *)mem_realloc(x->response_text, length + size * nmemb + 1);

	if (n) {
		x->response_text = n;
	} else {
		return 0;
	}
	memcpy(x->response_text + length, ptr, (size * nmemb));
	x->response_text[length + size * nmemb] = '\0';
	x->response_length += size * nmemb;

	return nmemb;
}

static JSValue
xhr_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

	if (!x) {
		return JS_EXCEPTION;
	}

	if (!x->sent) {
		JSValue arg = argv[0];

		if (x->async && x->method == POST && JS_IsString(arg)) {
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

			if (!x->async) {
#ifdef CONFIG_LIBCURL
				char *url = get_uri_string(x->uri, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD);

				if (!url) {
					return JS_UNDEFINED;
				}

				x->sent = true;
				/* init the curl session */
				CURL *curl_handle = curl_easy_init();
				/* set URL to get here */
				curl_easy_setopt(curl_handle, CURLOPT_URL, url);
				curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
				/* disable progress meter, set to 0L to enable it */
				curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
				/* send all data to this function  */
				curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
				/* write the page body to this file handle */
				curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, x);

				JSValue arg = argv[0];
				if (JS_IsString(arg)) {
					size_t size;
					const char *body = JS_ToCStringLen(ctx, &size, arg);

					if (body) {
						curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long) size);
						curl_easy_setopt(curl_handle, CURLOPT_COPYPOSTFIELDS, body);

						JS_FreeCString(ctx, body);
					}
				}

				/* get it! */
				curl_easy_perform(curl_handle);
				/* cleanup curl stuff */
				curl_easy_cleanup(curl_handle);
				x->ready_state = XHR_RSTATE_DONE;
				x->status = 200;
				mem_free(url);
				return JS_UNDEFINED;
#else
				return JS_EXCEPTION;
#endif
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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct Xhr *x = xhr_get(ctx, this_val);

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

		set_xhr_header(normalized_value, h_name, x);
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
	ELOG
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
	ELOG
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
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto, obj;

	/* XHR class */
	JS_NewClassID(JS_GetRuntime(ctx), &xhr_class_id);
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
