/* The QuickJS URL object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
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
#include "js/ecmascript.h"
#include "js/quickjs.h"
#include "js/quickjs/url.h"
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

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_url_class_id;

struct eljs_url {
	struct uri uri;
	char *hash;
	char *host;
	char *pathname;
	char *port;
	char *protocol;
	char *search;
};

static
void js_url_finalizer(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	struct eljs_url *url = (struct eljs_url *)JS_GetOpaque(val, js_url_class_id);

	if (url) {
		char *uristring = url->uri.string;
		done_uri(&url->uri);
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

static JSValue
js_url_get_property_hash(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}

	struct string fragment;
	if (!init_string(&fragment)) {
		return JS_EXCEPTION;
	}

	if (url->uri.fragmentlen) {
		add_bytes_to_string(&fragment, url->uri.fragment, url->uri.fragmentlen);
	}

	JSValue ret = JS_NewStringLen(ctx, fragment.source, fragment.length);
	done_string(&fragment);

	RETURN_JS(ret);
}

static JSValue
js_url_get_property_host(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	char *str = get_uri_string(&url->uri, URI_HOST_PORT);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	RETURN_JS(ret);
}

static JSValue
js_url_get_property_hostname(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	char *str = get_uri_string(&url->uri, URI_HOST);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	RETURN_JS(ret);
}

static JSValue
js_url_get_property_href(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	char *str = get_uri_string(&url->uri, URI_ORIGINAL);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	RETURN_JS(ret);
}

static JSValue
js_url_get_property_origin(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	char *str = get_uri_string(&url->uri, URI_SERVER);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	RETURN_JS(ret);
}

static JSValue
js_url_get_property_pathname(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	struct string pathname;
	if (!init_string(&pathname)) {
		return JS_NULL;
	}
	const char *query = (const char *)memchr(url->uri.data, '?', url->uri.datalen);
	int len = (query ? query - url->uri.data : url->uri.datalen);

	add_char_to_string(&pathname, '/');
	add_bytes_to_string(&pathname, url->uri.data, len);

	JSValue ret = JS_NewStringLen(ctx, pathname.source, pathname.length);
	done_string(&pathname);

	RETURN_JS(ret);
}

static JSValue
js_url_get_property_port(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}

	struct string port;
	if (!init_string(&port)) {
		return JS_NULL;
	}
	if (url->uri.portlen) {
		add_bytes_to_string(&port, url->uri.port, url->uri.portlen);
	}
	JSValue ret = JS_NewStringLen(ctx, port.source, port.length);
	done_string(&port);

	RETURN_JS(ret);
}

static JSValue
js_url_get_property_protocol(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	struct string proto;
	if (!init_string(&proto)) {
		return JS_NULL;
	}

	/* Custom or unknown keep the URI untouched. */
	if (url->uri.protocol == PROTOCOL_UNKNOWN) {
		add_to_string(&proto, struri(&url->uri));
	} else {
		add_bytes_to_string(&proto, url->uri.string, url->uri.protocollen);
		add_char_to_string(&proto, ':');
	}
	JSValue ret = JS_NewStringLen(ctx, proto.source, proto.length);
	done_string(&proto);

	RETURN_JS(ret);
}

static JSValue
js_url_get_property_search(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	struct string search;

	if (!init_string(&search)) {
		return JS_NULL;
	}
	const char *query = (const char *)memchr(url->uri.data, '?', url->uri.datalen);

	if (query) {
		add_bytes_to_string(&search, query, strcspn(query, "#" POST_CHAR_S));
	}
	JSValue ret = JS_NewStringLen(ctx, search.source, search.length);
	done_string(&search);

	RETURN_JS(ret);
}

static JSValue
js_url_set_property_hash(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *hash = memacpy(str, len);

	mem_free_set(&url->hash, hash);

	if (hash) {
		url->uri.fragment = hash;
		url->uri.fragmentlen = len;
	}
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_url_set_property_host(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *host = memacpy(str, len);

	mem_free_set(&url->host, host);

	if (host) {
		url->uri.host = host;
		url->uri.hostlen = len;
	}
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_url_set_property_hostname(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *hostname = memacpy(str, len);

	mem_free_set(&url->host, hostname);

	if (hostname) {
		url->uri.host = hostname;
		url->uri.hostlen = len;
	}
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_url_set_property_href(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	done_uri(&url->uri);

	char *urlstring = memacpy(str, len);
	JS_FreeCString(ctx, str);

	if (!urlstring) {
		return JS_EXCEPTION;
	}
	int ret = parse_uri(&url->uri, urlstring);

	if (ret != URI_ERRNO_OK) {
		return JS_EXCEPTION;
	}

	return JS_UNDEFINED;
}

static JSValue
js_url_set_property_pathname(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *pathname = memacpy(str, len);
	JS_FreeCString(ctx, str);

	mem_free_set(&url->pathname, pathname);

	if (pathname) {
		url->uri.data = pathname;
		url->uri.datalen = len;
	}
	return JS_UNDEFINED;
}

static JSValue
js_url_set_property_port(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *port = memacpy(str, len);
	JS_FreeCString(ctx, str);

	mem_free_set(&url->port, port);

	if (port) {
		url->uri.port = port;
		url->uri.portlen = strlen(port);
	}
	return JS_UNDEFINED;
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

static JSValue
js_url_set_property_protocol(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *protocol = memacpy(str, len);
	JS_FreeCString(ctx, str);

	mem_free_set(&url->protocol, protocol);

	if (protocol) {
		url->uri.protocollen = get_protocol_length(protocol);
		/* Figure out whether the protocol is known */
		url->uri.protocol = get_protocol(protocol, url->uri.protocollen);
	}

	return JS_UNDEFINED;
}

static JSValue
js_url_set_property_search(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct eljs_url *url = (struct eljs_url *)(JS_GetOpaque(this_val, js_url_class_id));

	if (!url) {
		return JS_NULL;
	}
	size_t len;
	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	char *search = memacpy(str, len);
	JS_FreeCString(ctx, str);

	mem_free_set(&url->search, search);

	if (search) {
		// TODO
	}
	return JS_UNDEFINED;
}

static JSValue
js_url_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return js_url_get_property_href(ctx, this_val);
}

static JSClassDef js_url_class = {
	"URL",
	js_url_finalizer
};

static const JSCFunctionListEntry js_url_proto_funcs[] = {
	JS_CGETSET_DEF("hash",	js_url_get_property_hash, js_url_set_property_hash),
	JS_CGETSET_DEF("host",	js_url_get_property_host, js_url_set_property_host),
	JS_CGETSET_DEF("hostname",	js_url_get_property_hostname, js_url_set_property_hostname),
	JS_CGETSET_DEF("href",	js_url_get_property_href, js_url_set_property_href),
	JS_CGETSET_DEF("origin",	js_url_get_property_origin, NULL),
	JS_CGETSET_DEF("pathname",	js_url_get_property_pathname, js_url_set_property_pathname),
	JS_CGETSET_DEF("port",	js_url_get_property_port, js_url_set_property_port),
	JS_CGETSET_DEF("protocol",	js_url_get_property_protocol, js_url_set_property_protocol),
	JS_CGETSET_DEF("search",	js_url_get_property_search, js_url_set_property_search),
	JS_CFUNC_DEF("toString", 0, js_url_toString),
};

static JSValue
js_url_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(new_target);

	JSValue obj = JS_NewObjectClass(ctx, js_url_class_id);
	REF_JS(obj);

	if (JS_IsException(obj)) {
		return obj;
	}
	struct eljs_url *url = (struct eljs_url *)mem_calloc(1, sizeof(*url));

	if (!url) {
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}

	if (argc > 0) {
		const char *str;
		size_t len;

		str = JS_ToCStringLen(ctx, &len, argv[0]);

		if (!str) {
			JS_FreeValue(ctx, obj);
			return JS_EXCEPTION;
		}
		char *urlstring = memacpy(str, len);
		JS_FreeCString(ctx, str);

		if (!urlstring) {
			return JS_EXCEPTION;
		}
		int ret = parse_uri(&url->uri, urlstring);

		if (ret != URI_ERRNO_OK) {
			JS_FreeValue(ctx, obj);
			return JS_EXCEPTION;
		}
	}
	JS_SetOpaque(obj, url);

	return obj;
}

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
js_url_init(JSContext *ctx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue proto;

	/* url class */
	JS_NewClassID(&js_url_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_url_class_id, &js_url_class);
	proto = JS_NewObject(ctx);
	REF_JS(proto);

	JS_SetPropertyFunctionList(ctx, proto, js_url_proto_funcs, countof(js_url_proto_funcs));
	JS_SetClassProto(ctx, js_url_class_id, proto);

	/* url object */
	(void)JS_NewGlobalCConstructor(ctx, "URL", js_url_constructor, 1, proto);
	//REF_JS(obj);

	return 0;
}
