/* The QuickJS location object implementation. */

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
#include "ecmascript/ecmascript.h"
#include "ecmascript/quickjs/location.h"
#include "ecmascript/quickjs/window.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
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

static JSClassID js_location_class_id;

static JSValue
js_location_get_property_hash(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	struct string fragment;
	init_string(&fragment);

	if (vs->uri->fragmentlen) {
		add_bytes_to_string(&fragment, vs->uri->fragment, vs->uri->fragmentlen);
	}

	JSValue ret = JS_NewString(ctx, fragment.source);
	done_string(&fragment);

	return ret;
}

static JSValue
js_location_get_property_host(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	char *str = get_uri_string(vs->uri, URI_HOST | URI_PORT);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	return ret;
}

static JSValue
js_location_get_property_hostname(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	char *str = get_uri_string(vs->uri, URI_HOST);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	return ret;
}

static JSValue
js_location_get_property_href(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	char *str = get_uri_string(vs->uri, URI_ORIGINAL);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	return ret;
}

static JSValue
js_location_get_property_origin(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	char *str = get_uri_string(vs->uri, URI_SERVER);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	JSValue ret = JS_NewString(ctx, str);
	mem_free(str);

	return ret;
}

static JSValue
js_location_get_property_pathname(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	struct string pathname;
	init_string(&pathname);

	const char *query = memchr(vs->uri->data, '?', vs->uri->datalen);
	int len = (query ? query - vs->uri->data : vs->uri->datalen);

	add_bytes_to_string(&pathname, vs->uri->data, len);

	JSValue ret = JS_NewString(ctx, pathname.source);
	done_string(&pathname);

	return ret;
}

static JSValue
js_location_get_property_port(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	struct string port;
	init_string(&port);
	if (vs->uri->portlen) {
		add_bytes_to_string(&port, vs->uri->port, vs->uri->portlen);
	}

	JSValue ret = JS_NewString(ctx, port.source);
	done_string(&port);

	return ret;
}

static JSValue
js_location_get_property_protocol(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	struct string proto;
	init_string(&proto);

	/* Custom or unknown keep the URI untouched. */
	if (vs->uri->protocol == PROTOCOL_UNKNOWN) {
		add_to_string(&proto, struri(vs->uri));
	} else {
		add_bytes_to_string(&proto, vs->uri->string, vs->uri->protocollen);
		add_char_to_string(&proto, ':');
	}

	JSValue ret = JS_NewString(ctx, proto.source);
	done_string(&proto);

	return ret;
}

static JSValue
js_location_get_property_search(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	struct string search;
	init_string(&search);

	const char *query = memchr(vs->uri->data, '?', vs->uri->datalen);

	if (query) {
		add_bytes_to_string(&search, query, strcspn(query, "#" POST_CHAR_S));
	}

	JSValue ret = JS_NewString(ctx, search.source);
	done_string(&search);

	return ret;
}

static JSValue
js_location_set_property_hash(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}

	size_t len;

	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	location_goto(vs->doc_view, str);
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_location_set_property_host(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}
	size_t len;

	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	location_goto(vs->doc_view, str);
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_location_set_property_hostname(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}
	size_t len;

	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	location_goto(vs->doc_view, str);
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_location_set_property_href(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}
	size_t len;

	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	location_goto(vs->doc_view, str);
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_location_set_property_pathname(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}
	size_t len;

	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	location_goto(vs->doc_view, str);
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_location_set_property_port(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}
	size_t len;

	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	location_goto(vs->doc_view, str);
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_location_set_property_protocol(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}
	size_t len;

	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	location_goto(vs->doc_view, str);
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_location_set_property_search(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}
	size_t len;

	const char *str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	location_goto(vs->doc_view, str);
	JS_FreeCString(ctx, str);

	return JS_UNDEFINED;
}

static JSValue
js_location_reload(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_EXCEPTION;
	}
	location_goto(vs->doc_view, "");

	return JS_UNDEFINED;
}

/* @location_funcs{"toString"}, @location_funcs{"toLocaleString"} */
static JSValue
js_location_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return js_location_get_property_href(ctx, this_val);
}

static const JSCFunctionListEntry js_location_proto_funcs[] = {
	JS_CGETSET_DEF("hash", js_location_get_property_hash, js_location_set_property_hash),
	JS_CGETSET_DEF("host", js_location_get_property_host, js_location_set_property_host),
	JS_CGETSET_DEF("hostname", js_location_get_property_hostname, js_location_set_property_hostname),
	JS_CGETSET_DEF("href", js_location_get_property_href, js_location_set_property_href),
	JS_CGETSET_DEF("origin", js_location_get_property_origin, nullptr),
	JS_CGETSET_DEF("pathname", js_location_get_property_pathname, js_location_set_property_pathname),
	JS_CGETSET_DEF("port", js_location_get_property_port, js_location_set_property_port),
	JS_CGETSET_DEF("protocol", js_location_get_property_protocol, js_location_set_property_protocol),
	JS_CGETSET_DEF("search", js_location_get_property_search, js_location_set_property_search),
	JS_CFUNC_DEF("reload", 0, js_location_reload),
	JS_CFUNC_DEF("toString", 0, js_location_toString),
	JS_CFUNC_DEF("toLocaleString", 0, js_location_toString),
};

static JSClassDef js_location_class = {
	"location",
};

static JSValue
js_location_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_location_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	return obj;

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_location_init(JSContext *ctx, JSValue global_obj)
{
	JSValue location_proto, location_class;

	/* create the location class */
	JS_NewClassID(&js_location_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_location_class_id, &js_location_class);

	location_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, location_proto, js_location_proto_funcs, countof(js_location_proto_funcs));

	location_class = JS_NewCFunction2(ctx, js_location_ctor, "location", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, location_class, location_proto);
	JS_SetClassProto(ctx, js_location_class_id, location_proto);

	JS_SetPropertyStr(ctx, global_obj, "location", location_proto);
	return 0;
}
