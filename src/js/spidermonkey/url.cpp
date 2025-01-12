/* The SpiderMonkey URL object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/spidermonkey/util.h"
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
#include "js/ecmascript.h"
#include "js/spidermonkey.h"
#include "js/spidermonkey/heartbeat.h"
#include "js/spidermonkey/url.h"
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

#include <iostream>
#include <list>
#include <map>
#include <utility>
#include <sstream>
#include <vector>

struct eljs_url {
	struct uri *uri;
	char *hash;
	char *host;
	char *pathname;
	char *port;
	char *protocol;
	char *search;
};

static bool url_get_property_hash(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_set_property_hash(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_get_property_host(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_set_property_host(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_get_property_hostname(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_set_property_hostname(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_get_property_href(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_set_property_href(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_get_property_origin(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_get_property_pathname(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_set_property_pathname(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_get_property_port(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_set_property_port(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_get_property_protocol(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_set_property_protocol(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_get_property_search(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool url_set_property_search(JSContext *ctx, unsigned int argc, JS::Value *vp);


static void
url_finalize(JS::GCContext *op, JSObject *url_obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(url_obj, 0);

	if (url) {
		char *uristring = url->uri->string;
		done_uri(url->uri);
		mem_free_if(uristring);
		mem_free_if(url->hash);
		mem_free_if(url->host);
		mem_free_if(url->pathname);
		mem_free_if(url->port);
		mem_free_if(url->protocol);
		mem_free_if(url->search);
		mem_free(url);
	}

}

JSClassOps url_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	url_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook // trace
};

JSClass url_class = {
	"URL",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&url_ops
};

bool
url_constructor(JSContext* ctx, unsigned argc, JS::Value* vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject newObj(ctx, JS_NewObjectForConstructor(ctx, &url_class, args));
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	if (!newObj) {
		return false;
	}
	struct eljs_url *url = (struct eljs_url *)mem_calloc(1, sizeof(*url));

	if (!url) {
		return false;
	}
	JS::SetReservedSlot(newObj, 0, JS::PrivateValue(url));

	if (argc > 0) {
		char *urlstring = jsval_to_string(ctx, args[0]);

		if (!urlstring) {
			return false;
		}
		url->uri = get_uri(urlstring, 0);

		if (!url->uri) {
			mem_free(urlstring);
			return false;
		}
	}

	args.rval().setObject(*newObj);

	return true;
}

static bool
url_get_property_hash(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct string fragment;
	if (!init_string(&fragment)) {
		return false;
	}

	if (url->uri->fragmentlen) {
		add_bytes_to_string(&fragment, url->uri->fragment, url->uri->fragmentlen);
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, fragment.source));
	done_string(&fragment);

	return true;
}

static bool
url_get_property_host(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *str = get_uri_string(url->uri, URI_HOST_PORT);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, str));
	mem_free(str);

	return true;
}

static bool
url_get_property_hostname(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *str = get_uri_string(url->uri, URI_HOST);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, str));
	mem_free(str);

	return true;
}

static bool
url_get_property_href(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *str = get_uri_string(url->uri, URI_ORIGINAL);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, str));
	mem_free(str);

	return true;
}

static bool
url_get_property_origin(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *str = get_uri_string(url->uri, URI_SERVER);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, str));
	mem_free(str);

	return true;
}

static bool
url_get_property_pathname(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct string pathname;
	if (!init_string(&pathname)) {
		return false;
	}
	const char *query = (const char *)memchr(url->uri->data, '?', url->uri->datalen);
	int len = (query ? query - url->uri->data : url->uri->datalen);

	add_char_to_string(&pathname, '/');
	add_bytes_to_string(&pathname, url->uri->data, len);
	args.rval().setString(JS_NewStringCopyZ(ctx, pathname.source));
	done_string(&pathname);

	return true;
}

static bool
url_get_property_port(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct string port;
	if (!init_string(&port)) {
		return false;
	}
	if (url->uri->portlen) {
		add_bytes_to_string(&port, url->uri->port, url->uri->portlen);
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, port.source));
	done_string(&port);

	return true;
}

static bool
url_get_property_protocol(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct string proto;
	if (!init_string(&proto)) {
		return false;
	}

	/* Custom or unknown keep the URI untouched. */
	if (url->uri->protocol == PROTOCOL_UNKNOWN) {
		add_to_string(&proto, struri(url->uri));
	} else {
		add_bytes_to_string(&proto, url->uri->string, url->uri->protocollen);
		add_char_to_string(&proto, ':');
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, proto.source));
	done_string(&proto);

	return true;
}

static bool
url_get_property_search(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct string search;

	if (!init_string(&search)) {
		return false;
	}
	const char *query = (const char *)memchr(url->uri->data, '?', url->uri->datalen);

	if (query) {
		add_bytes_to_string(&search, query, strcspn(query, "#" POST_CHAR_S));
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, search.source));
	done_string(&search);

	return true;
}

static bool
url_set_property_hash(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *hash = jsval_to_string(ctx, args[0]);

	mem_free_set(&url->hash, hash);

	if (hash) {
		url->uri->fragment = hash;
		url->uri->fragmentlen = strlen(hash);
	}
	args.rval().setUndefined();

	return true;
}

static bool
url_set_property_host(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *host = jsval_to_string(ctx, args[0]);

	mem_free_set(&url->host, host);

	if (host) {
		url->uri->host = host;
		url->uri->hostlen = strlen(host);
	}
	args.rval().setUndefined();

	return true;
}

static bool
url_set_property_hostname(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *hostname = jsval_to_string(ctx, args[0]);

	mem_free_set(&url->host, hostname);

	if (hostname) {
		url->uri->host = hostname;
		url->uri->hostlen = strlen(hostname);
	}
	args.rval().setUndefined();

	return true;
}

static bool
url_set_property_href(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	done_uri(url->uri);
	char *urlstring = jsval_to_string(ctx, args[0]);

	if (!urlstring) {
		return false;
	}
	url->uri = get_uri(urlstring, 0);
	args.rval().setUndefined();

	return true;
}

static bool
url_set_property_pathname(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *pathname = jsval_to_string(ctx, args[0]);

	mem_free_set(&url->pathname, pathname);

	if (pathname) {
		url->uri->data = pathname;
		url->uri->datalen = strlen(pathname);
	}
	args.rval().setUndefined();

	return true;
}

static bool
url_set_property_port(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *port = jsval_to_string(ctx, args[0]);

	mem_free_set(&url->port, port);

	if (port) {
		url->uri->port = port;
		url->uri->portlen = strlen(port);
	}
	args.rval().setUndefined();

	return true;
}

static inline int
get_protocol_length(const char *url)
{
	char *end = (char *) url;

	/* Seek the end of the protocol name if any. */
	/* RFC1738:
	 * scheme  = 1*[ lowalpha | digit | "+" | "-" | "." ]
	 * (but per its recommendations we accept "upalpha" too) */
	while (isalnum(*end) || *end == '+' || *end == '-' || *end == '.')
		end++;

	/* Now we make something to support our "IP version in protocol scheme
	 * name" hack and silently chop off the last digit if it's there. The
	 * IETF's not gonna notice I hope or it'd be going after us hard. */
	if (end != url && isdigit(end[-1]))
		end--;

	/* Also return 0 if there's no protocol name (@end == @url). */
	return (*end == ':' || isdigit(*end)) ? end - url : 0;
}

static bool
url_set_property_protocol(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *protocol = jsval_to_string(ctx, args[0]);

	mem_free_set(&url->protocol, protocol);

	if (protocol) {
		url->uri->protocollen = get_protocol_length(protocol);
		/* Figure out whether the protocol is known */
		url->uri->protocol = get_protocol(protocol, url->uri->protocollen);
	}
	args.rval().setUndefined();

	return true;
}

static bool
url_set_property_search(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &url_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	struct eljs_url *url = JS::GetMaybePtrFromReservedSlot<struct eljs_url>(hobj, 0);

	if (!url) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	char *search = jsval_to_string(ctx, args[0]);

	mem_free_set(&url->search, search);

	if (search) {
		// TODO
	}
	args.rval().setUndefined();

	return true;
}


JSPropertySpec url_props[] = {
	JS_PSGS("hash",	url_get_property_hash, url_set_property_hash, JSPROP_ENUMERATE),
	JS_PSGS("host",	url_get_property_host, url_set_property_host, JSPROP_ENUMERATE),
	JS_PSGS("hostname",	url_get_property_hostname, url_set_property_hostname, JSPROP_ENUMERATE),
	JS_PSGS("href",	url_get_property_href, url_set_property_href, JSPROP_ENUMERATE),
	JS_PSG("origin",	url_get_property_origin, JSPROP_ENUMERATE),
	JS_PSGS("pathname",	url_get_property_pathname, url_set_property_pathname, JSPROP_ENUMERATE),
	JS_PSGS("port",	url_get_property_port, url_set_property_port, JSPROP_ENUMERATE),
	JS_PSGS("protocol",	url_get_property_protocol, url_set_property_protocol, JSPROP_ENUMERATE),
	JS_PSGS("search",	url_get_property_search, url_set_property_search, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
url_toString(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue r_val(ctx, val);
	bool ret = JS_GetProperty(ctx, hobj, "href", &r_val);

	args.rval().set(val);
	return ret;
}

const spidermonkeyFunctionSpec url_funcs[] = {
	{ "toString",		url_toString,	0 },
	{ NULL }
};
