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


struct xhr {
	struct download download;
	struct ecmascript_interpreter *interpreter;
	JS::RootedObject thisval;
	JS::RootedValue onabort;
	JS::RootedValue onerror;
	JS::RootedValue onload;
	JS::RootedValue onloadend;
	JS::RootedValue onloadstart;
	JS::RootedValue onprogress;
	JS::RootedValue onreadystatechange;
	JS::RootedValue ontimeout;
	char *response;
	char *responseText;
	char *responseType;
	char *responseURL;
	char *statusText;
	char *upload;
	bool withCredentials;
	int method;
	int readyState;
	int status;
	int timeout;
};

static void
xhr_finalize(JSFreeOp *op, JSObject *xhr_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct xhr *xhr = (struct xhr *)JS::GetPrivate(xhr_obj);

	if (xhr) {
		mem_free_if(xhr->response);
		mem_free_if(xhr->responseText);
		mem_free_if(xhr->responseType);
		mem_free_if(xhr->responseURL);
		mem_free_if(xhr->statusText);
		mem_free_if(xhr->upload);
		mem_free(xhr);

		JS::SetPrivate(xhr_obj, nullptr);
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
	nullptr,  // hasInstance
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass xhr_class = {
	"XMLHttpRequest",
	JSCLASS_HAS_PRIVATE,
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
	xhr->interpreter = interpreter;
	xhr->thisval = newObj;
	JS::SetPrivate(newObj, xhr);
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

static bool xhr_abort(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_getAllResponseHeaders(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_getResponseHeader(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_open(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_overrideMimeType(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_send(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool xhr_setRequestHeader(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec xhr_funcs[] = {
	{ "abort",	xhr_abort,		0 },
	{ "getAllResponseHeaders",	xhr_getAllResponseHeaders,		0 },
	{ "getResponseHeader",	xhr_getResponseHeader,		1 },
	{ "open",	xhr_open,		5 },
	{ "overrideMimeType",	xhr_overrideMimeType,		1 },
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
//	JS::RootedObject hobj(ctx, &args.thisv().toObject());
/// TODO
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
//	JS::RootedObject hobj(ctx, &args.thisv().toObject());
/// TODO

	args.rval().setUndefined();
	return true;
}

static bool
xhr_getResponseHeader(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

static bool
xhr_open(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
		JS::Realm *comp = JS::EnterRealm(ctx, (JSObject *)interpreter->ac);
		JS::RootedValue r_val(ctx);
		interpreter->heartbeat = add_heartbeat(interpreter);
		JS_CallFunctionValue(ctx, xhr->thisval, xhr->onload, JS::HandleValueArray::empty(), &r_val);
		done_heartbeat(interpreter->heartbeat);
		JS::LeaveRealm(ctx, comp);

		check_for_rerender(interpreter, "xhr_onload");
	}
}

static void
xhr_loading_callback(struct download *download, struct xhr *xhr)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (is_in_result_state(download->state)) {
		struct cache_entry *cached = download->cached;

		if (!cached) {
			return;
		}
		struct fragment *fragment = get_cache_fragment(cached);

		if (!fragment) {
			return;
		}
		mem_free_set(&xhr->responseText, memacpy(fragment->data, fragment->length));
		xhr->status = 200;
		mem_free_set(&xhr->statusText, stracpy("OK"));
		register_bottom_half(onload_run, xhr);
	}
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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));
	struct view_state *vs;
	struct document_view *doc_view;
	vs = interpreter->vs;
	doc_view = vs->doc_view;

	if (!xhr) {
		return false;
	}
	xhr->download.data = xhr;
	xhr->download.callback = (download_callback_T *)xhr_loading_callback;

	struct uri *uri = NULL;

	if (!strchr(xhr->responseURL, '/')) {
		char *ref = get_uri_string(vs->uri, URI_DIR_LOCATION | URI_PATH);

		if (ref) {
			char *slash = strrchr(ref, '/');

			if (slash) {
				*slash = '\0';
			}
			char *url = straconcat(ref, "/", xhr->responseURL, NULL);

			if (url) {
				uri = get_uri(url, URI_NONE);
				mem_free(url);
			}
			mem_free(ref);
		}
	}
	if (!uri) {
		uri = get_uri(xhr->responseURL, URI_NONE);
	}

	if (uri) {
		load_uri(uri, doc_view->session->referrer, &xhr->download, PRI_MAIN, CACHE_MODE_NORMAL, -1);
		done_uri(uri);
	}
	args.rval().setUndefined();

	return true;
}

static bool
xhr_setRequestHeader(JSContext *ctx, unsigned int argc, JS::Value *rval)
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

static bool
xhr_get_property_onabort(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

	if (!xhr || !xhr->responseText) {
		args.rval().setNull();
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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

	if (!xhr) {
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
	struct xhr *xhr = (struct xhr *)(JS::GetPrivate(hobj));

	if (!xhr) {
		return false;
	}
	xhr->withCredentials = args[0].toBoolean();

	return true;
}