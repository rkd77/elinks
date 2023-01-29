/* The SpiderMonkey xhr object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"
#include <js/BigInt.h>
#include <js/Conversions.h>

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
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/heartbeat.h"
#include "ecmascript/spidermonkey/xhr.h"
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

#include <curl/curl.h>

#include <iostream>
#include <list>
#include <map>
#include <utility>
#include <sstream>
#include <vector>

const unsigned short UNSENT = 0;
const unsigned short OPENED = 1;
const unsigned short HEADERS_RECEIVED = 2;
const unsigned short LOADING = 3;
const unsigned short DONE = 4;

static bool xhr_get_property_onabort(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_onerror(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_onload(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_onloadend(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_onloadstart(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_onprogress(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_onreadystatechange(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_ontimeout(JSContext *cx, unsigned int argc, JS::Value *vp);

static bool xhr_get_property_readyState(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_response(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_responseText(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_responseType(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_responseURL(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_status(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_statusText(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_timeout(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_upload(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_get_property_withCredentials(JSContext *cx, unsigned int argc, JS::Value *vp);

static bool xhr_set_property_onabort(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_set_property_onerror(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_set_property_onload(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_set_property_onloadend(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_set_property_onloadstart(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_set_property_onprogress(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_set_property_onreadystatechange(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_set_property_ontimeout(JSContext *cx, unsigned int argc, JS::Value *vp);

static bool xhr_set_property_responseType(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_set_property_timeout(JSContext *cx, unsigned int argc, JS::Value *vp);
static bool xhr_set_property_withCredentials(JSContext *cx, unsigned int argc, JS::Value *vp);

static char *normalize(char *value);


enum {
    GET = 1,
    HEAD = 2,
    POST = 3
};

struct listener {
	LIST_HEAD(struct listener);
	char *typ;
	JS::RootedValue fun;
};

struct classcomp {
	bool operator() (const std::string& lhs, const std::string& rhs) const
	{
		return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};

struct xhr {
	std::map<std::string, std::string> requestHeaders;
	std::map<std::string, std::string, classcomp> responseHeaders;
	struct download download;
	struct ecmascript_interpreter *interpreter;
	JS::RootedObject thisval;

	LIST_OF(struct listener) listeners;

	JS::RootedValue onabort;
	JS::RootedValue onerror;
	JS::RootedValue onload;
	JS::RootedValue onloadend;
	JS::RootedValue onloadstart;
	JS::RootedValue onprogress;
	JS::RootedValue onreadystatechange;
	JS::RootedValue ontimeout;
	struct uri *uri;
	char *response;
	char *responseText;
	char *responseType;
	char *responseURL;
	char *statusText;
	char *upload;
	bool async;
	bool withCredentials;
	bool isSend;
	bool isUpload;
	int method;
	int status;
	int timeout;
	unsigned short readyState;
};

static void onload_run(void *data);
static void onloadend_run(void *data);
static void onreadystatechange_run(void *data);
static void ontimeout_run(void *data);

static void
xhr_finalize(JS::GCContext *op, JSObject *xhr_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(xhr_obj, 0);

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
		xhr->responseHeaders.clear();
		xhr->requestHeaders.clear();

		struct listener *l;

		foreach(l, xhr->listeners) {
			mem_free_set(&l->typ, NULL);
		}
		free_list(xhr->listeners);
		mem_free(xhr);

		JS::SetReservedSlot(xhr_obj, 0, JS::UndefinedValue());
	}
}

JSClassOps xhr_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	xhr_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass xhr_class = {
	"XMLHttpRequest",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&xhr_ops
};

bool
xhr_constructor(JSContext* ctx, unsigned argc, JS::Value* vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject newObj(ctx, JS_NewObjectForConstructor(ctx, &xhr_class, args));
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!newObj) {
		return false;
	}
	struct xhr *xhr = (struct xhr *)mem_calloc(1, sizeof(*xhr));
	init_list(xhr->listeners);
	xhr->interpreter = interpreter;
	xhr->thisval = newObj;
	xhr->async = true;
	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(xhr));
	args.rval().setObject(*newObj);

	return true;
}

JSPropertySpec xhr_props[] = {
	JS_PSGS("onabort",	xhr_get_property_onabort, xhr_set_property_onabort, JSPROP_ENUMERATE),
	JS_PSGS("onerror",	xhr_get_property_onerror, xhr_set_property_onerror, JSPROP_ENUMERATE),
	JS_PSGS("onload",	xhr_get_property_onload, xhr_set_property_onload, JSPROP_ENUMERATE),
	JS_PSGS("onloadend",	xhr_get_property_onloadend, xhr_set_property_onloadend, JSPROP_ENUMERATE),
	JS_PSGS("onloadstart",	xhr_get_property_onloadstart, xhr_set_property_onloadstart, JSPROP_ENUMERATE),
	JS_PSGS("onprogress",	xhr_get_property_onprogress, xhr_set_property_onprogress, JSPROP_ENUMERATE),
	JS_PSGS("onreadystatechange",	xhr_get_property_onreadystatechange, xhr_set_property_onreadystatechange, JSPROP_ENUMERATE),
	JS_PSGS("ontimeout",	xhr_get_property_ontimeout, xhr_set_property_ontimeout, JSPROP_ENUMERATE),
	JS_PSG("readyState",	xhr_get_property_readyState, JSPROP_ENUMERATE),
	JS_PSG("response",	xhr_get_property_response, JSPROP_ENUMERATE),
	JS_PSG("responseText",	xhr_get_property_responseText, JSPROP_ENUMERATE),
	JS_PSGS("responseType",	xhr_get_property_responseType, xhr_set_property_responseType, JSPROP_ENUMERATE),
	JS_PSG("responseURL",	xhr_get_property_responseURL, JSPROP_ENUMERATE),
	JS_PSG("status",	xhr_get_property_status, JSPROP_ENUMERATE),
	JS_PSG("statusText",	xhr_get_property_statusText, JSPROP_ENUMERATE),
	JS_PSGS("timeout",	xhr_get_property_timeout, xhr_set_property_timeout, JSPROP_ENUMERATE),
	JS_PSG("upload",	xhr_get_property_upload, JSPROP_ENUMERATE),
	JS_PSGS("withCredentials",xhr_get_property_withCredentials, xhr_set_property_withCredentials, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
xhr_static_get_property_UNSENT(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(UNSENT);

	return true;
}

static bool
xhr_static_get_property_OPENED(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(OPENED);

	return true;
}

static bool
xhr_static_get_property_HEADERS_RECEIVED(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(HEADERS_RECEIVED);

	return true;
}

static bool
xhr_static_get_property_LOADING(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(LOADING);

	return true;
}


static bool
xhr_static_get_property_DONE(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	args.rval().setInt32(DONE);

	return true;
}

JSPropertySpec xhr_static_props[] = {
	JS_PSG("UNSENT",	xhr_static_get_property_UNSENT, JSPROP_PERMANENT ),
	JS_PSG("OPENED",	xhr_static_get_property_OPENED, JSPROP_PERMANENT),
	JS_PSG("HEADERS_RECEIVED",	xhr_static_get_property_HEADERS_RECEIVED, JSPROP_PERMANENT),
	JS_PSG("LOADING",	xhr_static_get_property_LOADING, JSPROP_PERMANENT),
	JS_PSG("DONE",	xhr_static_get_property_DONE, JSPROP_PERMANENT),
	JS_PS_END
};

static bool xhr_abort(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_getAllResponseHeaders(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_getResponseHeader(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_open(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_overrideMimeType(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_send(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_setRequestHeader(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec xhr_funcs[] = {
	{ "abort",	xhr_abort,		0 },
	{ "addEventListener", xhr_addEventListener, 3 },
	{ "getAllResponseHeaders",	xhr_getAllResponseHeaders,		0 },
	{ "getResponseHeader",	xhr_getResponseHeader,		1 },
	{ "open",	xhr_open,		5 },
	{ "overrideMimeType",	xhr_overrideMimeType,		1 },
	{ "removeEventListener", xhr_removeEventListener, 3 },
	{ "send",	xhr_send,		1 },
	{ "setRequestHeader",	xhr_setRequestHeader,		2 },
	{ NULL }
};

static bool
xhr_abort(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (xhr && xhr->download.conn) {
		abort_connection(xhr->download.conn, connection_state(S_INTERRUPTED));
	}

	args.rval().setUndefined();
	return true;
}

static bool
xhr_addEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (argc < 2) {
		args.rval().setUndefined();
		return true;
	}
	char *method = jsval_to_string(ctx, args[0]);
	JS::RootedValue fun(ctx, args[1]);

	struct listener *l;

	foreach(l, xhr->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (l->fun == fun) {
			args.rval().setUndefined();
			mem_free(method);
			return true;
		}
	}
	struct listener *n = (struct listener *)mem_calloc(1, sizeof(*n));

	if (n) {
		n->typ = method;
		n->fun = fun;
		add_to_list_end(xhr->listeners, n);
	}
	args.rval().setUndefined();
	return true;
}

static bool
xhr_removeEventListener(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (argc < 2) {
		args.rval().setUndefined();
		return true;
	}
	char *method = jsval_to_string(ctx, args[0]);

	if (!method) {
		return false;
	}
	JS::RootedValue fun(ctx, args[1]);

	struct listener *l;

	foreach(l, xhr->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (l->fun == fun) {
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			mem_free(l);
			mem_free(method);
			args.rval().setUndefined();
			return true;
		}
	}
	mem_free(method);
	args.rval().setUndefined();
	return true;
}

static bool
xhr_getAllResponseHeaders(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	std::string output = "";

	for (auto h: xhr->responseHeaders) {
		output += h.first + ": " + h.second + "\r\n";
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, output.c_str()));
	return true;
}

static bool
xhr_getResponseHeader(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr || argc == 0) {
		return false;
	}
	char *header = jsval_to_string(ctx, args[0]);

	if (header) {
		std::string output = "";
		for (auto h: xhr->responseHeaders) {
			if (!strcasecmp(header, h.first.c_str())) {
				output = h.second;
				break;
			}
		}
		mem_free(header);
		if (!output.empty()) {
			args.rval().setString(JS_NewStringCopyZ(ctx, output.c_str()));
			return true;
		}
	}
	args.rval().setNull();
	return true;
}

static bool
xhr_open(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (argc < 2) {
		return false;
	}

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);
	struct view_state *vs = interpreter->vs;

	if (!xhr) {
		return false;
	}
	char *method = jsval_to_string(ctx, args[0]);

	if (!method) {
		return false;
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
	mem_free(method);

	if (!method_ok) {
		return false;
	}
	mem_free_set(&xhr->responseURL, jsval_to_string(ctx, args[1]));

	if (!xhr->responseURL) {
		return false;
	}

	if (!strchr(xhr->responseURL, '/')) {
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
	}
	if (!xhr->uri) {
		xhr->uri = get_uri(xhr->responseURL, URI_NONE);
	}

	if (!xhr->uri) {
		return false;
	}

	if (argc > 2) {
		xhr->async = args[2].toBoolean();
	} else {
		xhr->async = true;
	}
	char *username = NULL;
	char *password = NULL;

	if (argc > 3) {
		username = jsval_to_string(ctx, args[3]);
	}
	if (argc > 4) {
		password = jsval_to_string(ctx, args[4]);
	}
	if (username || password) {
		if (username) {
			xhr->uri->user = username;
			xhr->uri->userlen = strlen(username);
		}
		if (password) {
			xhr->uri->password = password;
			xhr->uri->passwordlen = strlen(password);
		}
		char *url2 = get_uri_string(xhr->uri, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD);
		mem_free_if(username);
		mem_free_if(password);

		if (!url2) {
			return false;
		}
		done_uri(xhr->uri);
		xhr->uri = get_uri(url2, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD);
		mem_free(url2);
	}
	if (!xhr->async && (xhr->timeout || (xhr->responseType && *(xhr->responseType)))) {
		return false;
	}

	// TODO terminate fetch
	xhr->isSend = false;
	xhr->isUpload = false;
	xhr->requestHeaders.clear();
	xhr->responseHeaders.clear();
	mem_free_set(&xhr->response, NULL);
	mem_free_set(&xhr->responseText, NULL);

	if (xhr->readyState != OPENED) {
		xhr->readyState = OPENED;
		register_bottom_half(onreadystatechange_run, xhr);
	}

	args.rval().setUndefined();

	return true;
}

static bool
xhr_overrideMimeType(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
//	JS::RootedObject hobj(ctx, &args.thisv().toObject());
/// TODO

	args.rval().setUndefined();
	return true;
}

static void
onload_run(void *data)
{
	struct xhr *xhr = (struct xhr *)data;

	if (xhr) {
		struct ecmascript_interpreter *interpreter = xhr->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());
		JS::RootedValue r_val(ctx);
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct listener *l;

		foreach(l, xhr->listeners) {
			if (strcmp(l->typ, "load")) {
				continue;
			}
			JS_CallFunctionValue(ctx, xhr->thisval, l->fun, JS::HandleValueArray::empty(), &r_val);
		}
		JS_CallFunctionValue(ctx, xhr->thisval, xhr->onload, JS::HandleValueArray::empty(), &r_val);
		done_heartbeat(interpreter->heartbeat);

		check_for_rerender(interpreter, "xhr_onload");
	}
}

static void
onloadend_run(void *data)
{
	struct xhr *xhr = (struct xhr *)data;

	if (xhr) {
		struct ecmascript_interpreter *interpreter = xhr->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());
		JS::RootedValue r_val(ctx);
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct listener *l;

		foreach(l, xhr->listeners) {
			if (strcmp(l->typ, "loadend")) {
				continue;
			}
			JS_CallFunctionValue(ctx, xhr->thisval, l->fun, JS::HandleValueArray::empty(), &r_val);
		}
		JS_CallFunctionValue(ctx, xhr->thisval, xhr->onloadend, JS::HandleValueArray::empty(), &r_val);
		done_heartbeat(interpreter->heartbeat);

		check_for_rerender(interpreter, "xhr_onloadend");
	}
}

static void
onreadystatechange_run(void *data)
{
	struct xhr *xhr = (struct xhr *)data;

	if (xhr) {
		struct ecmascript_interpreter *interpreter = xhr->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());
		JS::RootedValue r_val(ctx);
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct listener *l;

		foreach(l, xhr->listeners) {
			if (strcmp(l->typ, "readystatechange")) {
				continue;
			}
			JS_CallFunctionValue(ctx, xhr->thisval, l->fun, JS::HandleValueArray::empty(), &r_val);
		}
		JS_CallFunctionValue(ctx, xhr->thisval, xhr->onreadystatechange, JS::HandleValueArray::empty(), &r_val);
		done_heartbeat(interpreter->heartbeat);

		check_for_rerender(interpreter, "xhr_onreadystatechange");
	}
}

static void
ontimeout_run(void *data)
{
	struct xhr *xhr = (struct xhr *)data;

	if (xhr) {
		struct ecmascript_interpreter *interpreter = xhr->interpreter;
		JSContext *ctx = (JSContext *)interpreter->backend_data;
		JSAutoRealm ar(ctx, (JSObject *)interpreter->ac->get());
		JS::RootedValue r_val(ctx);
		interpreter->heartbeat = add_heartbeat(interpreter);

		struct listener *l;

		foreach(l, xhr->listeners) {
			if (strcmp(l->typ, "timeout")) {
				continue;
			}
			JS_CallFunctionValue(ctx, xhr->thisval, l->fun, JS::HandleValueArray::empty(), &r_val);
		}
		JS_CallFunctionValue(ctx, xhr->thisval, xhr->ontimeout, JS::HandleValueArray::empty(), &r_val);
		done_heartbeat(interpreter->heartbeat);

		check_for_rerender(interpreter, "xhr_ontimeout");
	}
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
xhr_loading_callback(struct download *download, struct xhr *xhr)
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

					for (auto h: xhr->responseHeaders) {
						const std::string hh = h.first;
						if (!strcasecmp(hh.c_str(), header)) {
							xhr->responseHeaders[hh] = xhr->responseHeaders[hh] + ", " + normalized_value;
							found = true;
							break;
						}
					}

					if (!found) {
						xhr->responseHeaders[header] = normalized_value;
					}
					mem_free(header);
					mem_free(value);
				}
			}
			xhr->status = status;
			mem_free_set(&xhr->statusText, stracpy(statusText.c_str()));
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
	struct xhr *xhr = (struct xhr *)stream;

	size_t length = 0;

	if (xhr->responseText) {
		length = strlen(xhr->responseText);
	}
	char *n = (char *)mem_realloc(xhr->responseText, length + size * nmemb + 1);

	if (n) {
		xhr->responseText = n;
	} else {
		return 0;
	}
	memcpy(xhr->responseText + length, ptr, (size * nmemb));
	xhr->responseText[length + size * nmemb] = '\0';

	return nmemb;
}

static bool
xhr_send(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);
	struct view_state *vs;
	struct document_view *doc_view;
	vs = interpreter->vs;
	doc_view = vs->doc_view;

	if (!xhr) {
		return false;
	}

	if (xhr->readyState != OPENED) {
		return false;
	}

	if (xhr->isSend) {
		return false;
	}

	char *body = NULL;

	if (xhr->method == POST && argc == 1) {
		body = jsval_to_string(ctx, args[0]);

		if (body) {
			struct string post;
			if (!init_string(&post)) {
				mem_free(body);
				return false;
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
				mem_free(body);
				return false;
			}
			done_uri(xhr->uri);
			xhr->uri = get_uri(url2, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD | URI_POST);
			mem_free(url2);
			mem_free(body);
		}
	}

	if (xhr->uri) {
		if (xhr->uri->protocol == PROTOCOL_FILE && !get_opt_bool("ecmascript.allow_xhr_file", NULL)) {
			args.rval().setUndefined();
			return true;
		}

		if (!xhr->async) {
			char *url = get_uri_string(xhr->uri, URI_DIR_LOCATION | URI_PATH | URI_USER | URI_PASSWORD);

			if (!url) {
				args.rval().setUndefined();
				return true;
			}

			xhr->isSend = true;
			CURL *curl_handle = curl_easy_init();
			curl_easy_setopt(curl_handle, CURLOPT_URL, url);
			curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
			curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
			curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
			curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, xhr);
			curl_easy_perform(curl_handle);
			curl_easy_cleanup(curl_handle);
			xhr->readyState = DONE;
			xhr->status = 200;
			mem_free(url);

			args.rval().setUndefined();
			return true;
		}
		xhr->download.data = xhr;
		xhr->download.callback = (download_callback_T *)xhr_loading_callback;
		load_uri(xhr->uri, doc_view->session->referrer, &xhr->download, PRI_MAIN, CACHE_MODE_NORMAL, -1);
		if (xhr->timeout) {
			set_connection_timeout_xhr(xhr->download.conn, xhr->timeout);
		}
	}
	args.rval().setUndefined();

	return true;
}

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

static bool
xhr_setRequestHeader(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (argc != 2) {
		return false;
	}

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}

	if (xhr->readyState != OPENED || xhr->isSend) {
		return false;
	}
	char *header = jsval_to_string(ctx, args[0]);

	if (!header) {
		return false;
	}
	char *value = jsval_to_string(ctx, args[1]);

	if (value) {
		char *normalized_value = normalize(value);

		if (!valid_header(header)) {
			mem_free(header);
			mem_free(value);
			return false;
		}

		if (forbidden_header(header)) {
			mem_free(header);
			mem_free(value);
			args.rval().setUndefined();
			return true;
		}

		bool found = false;
		for (auto h: xhr->requestHeaders) {
			const std::string hh = h.first;
			if (!strcasecmp(hh.c_str(), header)) {
				xhr->requestHeaders[hh] = xhr->requestHeaders[hh] + ", " + normalized_value;
				found = true;
				break;
			}
		}

		if (!found) {
			xhr->requestHeaders[header] = normalized_value;
		}

		mem_free(header);
		mem_free(value);
	}

	args.rval().setUndefined();
	return true;
}

static bool
xhr_get_property_onabort(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	args.rval().set(xhr->onabort);

	return true;
}

static bool
xhr_set_property_onabort(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	JS::RootedValue fun(ctx, args[0]);
	xhr->onabort = fun;

	return true;
}

static bool
xhr_get_property_onerror(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	args.rval().set(xhr->onerror);

	return true;
}

static bool
xhr_set_property_onerror(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	JS::RootedValue fun(ctx, args[0]);
	xhr->onerror = fun;

	return true;
}

static bool
xhr_get_property_onload(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	args.rval().set(xhr->onload);

	return true;
}

static bool
xhr_set_property_onload(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	JS::RootedValue fun(ctx, args[0]);
	xhr->onload = fun;

	return true;
}

static bool
xhr_get_property_onloadend(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	args.rval().set(xhr->onloadend);

	return true;
}

static bool
xhr_set_property_onloadend(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	JS::RootedValue fun(ctx, args[0]);
	xhr->onloadend = fun;

	return true;
}

static bool
xhr_get_property_onloadstart(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	args.rval().set(xhr->onloadstart);

	return true;
}

static bool
xhr_set_property_onloadstart(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	JS::RootedValue fun(ctx, args[0]);
	xhr->onloadstart = fun;

	return true;
}

static bool
xhr_get_property_onprogress(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	args.rval().set(xhr->onprogress);

	return true;
}

static bool
xhr_set_property_onprogress(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	JS::RootedValue fun(ctx, args[0]);
	xhr->onprogress = fun;

	return true;
}

static bool
xhr_get_property_onreadystatechange(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	args.rval().set(xhr->onreadystatechange);

	return true;
}

static bool
xhr_set_property_onreadystatechange(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	JS::RootedValue fun(ctx, args[0]);
	xhr->onreadystatechange = fun;

	return true;
}

static bool
xhr_get_property_ontimeout(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	args.rval().set(xhr->ontimeout);

	return true;
}

static bool
xhr_set_property_ontimeout(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	JS::RootedValue fun(ctx, args[0]);
	xhr->ontimeout = fun;

	return true;
}

static bool
xhr_get_property_readyState(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		args.rval().setNull();
		return true;
	}
	args.rval().setInt32(xhr->readyState);

	return true;
}

static bool
xhr_get_property_response(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr || !xhr->response) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, xhr->response));

	return true;
}

static bool
xhr_get_property_responseText(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}

	if (xhr->readyState != LOADING && xhr->readyState != DONE) {
		args.rval().setString(JS_NewStringCopyZ(ctx, ""));
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, xhr->responseText));

	return true;
}

static bool
xhr_get_property_responseType(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr || !xhr->responseType) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, xhr->responseType));

	return true;
}

static bool
xhr_get_property_responseURL(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr || !xhr->responseURL) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, xhr->responseURL));

	return true;
}

static bool
xhr_get_property_status(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		args.rval().setNull();
		return true;
	}
	args.rval().setInt32(xhr->status);

	return true;
}

static bool
xhr_get_property_statusText(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr || !xhr->statusText) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, xhr->statusText));

	return true;
}

static bool
xhr_get_property_upload(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr || !xhr->upload) {
		args.rval().setNull();
		return true;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, xhr->upload));

	return true;
}

static bool
xhr_get_property_timeout(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		args.rval().setNull();
		return true;
	}
	args.rval().setInt32(xhr->timeout);

	return true;
}

static bool
xhr_get_property_withCredentials(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		args.rval().setNull();
		return true;
	}
	args.rval().setBoolean(xhr->withCredentials);

	return true;
}

static bool
xhr_set_property_responseType(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}
	mem_free_set(&xhr->responseType, jsval_to_string(ctx, args[0]));

	return true;
}

static bool
xhr_set_property_timeout(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}

	if (!xhr->async) {
		return false;
	}

	xhr->timeout = args[0].toInt32();

	return true;
}

static bool
xhr_set_property_withCredentials(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = JS::GetMaybePtrFromReservedSlot<struct xhr>(hobj, 0);

	if (!xhr) {
		return false;
	}

	if (xhr->readyState != UNSENT && xhr->readyState != OPENED) {
		return false;
	}

	if (xhr->isSend) {
		return false;
	}
	xhr->withCredentials = args[0].toBoolean();
	/// TODO Set this’s cross-origin credentials to the given value.

	return true;
}
