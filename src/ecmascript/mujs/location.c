/* The MuJS location object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/location.h"
#include "ecmascript/mujs/window.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "viewer/text/vs.h"

static void
mjs_location_get_property_hash(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}

	struct string fragment;
	if (!init_string(&fragment)) {
		js_error(J, "out of memory");
		return;
	}

	if (vs->uri->fragmentlen) {
		add_bytes_to_string(&fragment, vs->uri->fragment, vs->uri->fragmentlen);
	}
	js_pushstring(J, fragment.source);
	done_string(&fragment);
}

static void
mjs_location_get_property_host(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}

	char *str = get_uri_string(vs->uri, URI_HOST_PORT);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!str");
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_location_get_property_hostname(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}

	char *str = get_uri_string(vs->uri, URI_HOST);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!str");
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_location_get_property_href(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}

	char *str = get_uri_string(vs->uri, URI_ORIGINAL);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!str");
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_location_get_property_origin(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}

	char *str = get_uri_string(vs->uri, URI_SERVER);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!str");
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_location_get_property_pathname(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}

	struct string pathname;
	if (!init_string(&pathname)) {
		js_error(J, "out of memory");
		return;
	}

	const char *query = (const char *)memchr(vs->uri->data, '?', vs->uri->datalen);
	int len = (query ? query - vs->uri->data : vs->uri->datalen);

	add_bytes_to_string(&pathname, vs->uri->data, len);
	js_pushstring(J, pathname.source);
	done_string(&pathname);
}

static void
mjs_location_get_property_port(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}

	struct string port;
	if (!init_string(&port)) {
		js_error(J, "out of memory");
		return;
	}
	if (vs->uri->portlen) {
		add_bytes_to_string(&port, vs->uri->port, vs->uri->portlen);
	}
	js_pushstring(J, port.source);
	done_string(&port);
}

static void
mjs_location_get_property_protocol(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}

	struct string proto;
	if (!init_string(&proto)) {
		js_error(J, "out of memory");
		return;
	}

	/* Custom or unknown keep the URI untouched. */
	if (vs->uri->protocol == PROTOCOL_UNKNOWN) {
		add_to_string(&proto, struri(vs->uri));
	} else {
		add_bytes_to_string(&proto, vs->uri->string, vs->uri->protocollen);
		add_char_to_string(&proto, ':');
	}
	js_pushstring(J, proto.source);
	done_string(&proto);
}

static void
mjs_location_get_property_search(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}

	struct string search;
	if (!init_string(&search)) {
		js_error(J, "out of memory");
		return;
	}

	const char *query = (const char *)memchr(vs->uri->data, '?', vs->uri->datalen);

	if (query) {
		add_bytes_to_string(&search, query, strcspn(query, "#" POST_CHAR_S));
	}
	js_pushstring(J, search.source);
	done_string(&search);
}

static void
mjs_location_set_property_hash(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	location_goto_const(vs->doc_view, str, 0);
	js_pushundefined(J);
}

static void
mjs_location_set_property_host(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	location_goto_const(vs->doc_view, str, 0);
	js_pushundefined(J);
}

static void
mjs_location_set_property_hostname(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	location_goto_const(vs->doc_view, str, 0);
	js_pushundefined(J);
}

static void
mjs_location_set_property_href(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	location_goto_const(vs->doc_view, str, 0);
	js_pushundefined(J);
}

static void
mjs_location_set_property_pathname(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	location_goto_const(vs->doc_view, str, 0);
	js_pushundefined(J);
}

static void
mjs_location_set_property_port(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	location_goto_const(vs->doc_view, str, 0);
	js_pushundefined(J);
}

static void
mjs_location_set_property_protocol(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	location_goto_const(vs->doc_view, str, 0);
	js_pushundefined(J);
}

static void
mjs_location_set_property_search(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	location_goto_const(vs->doc_view, str, 0);
	js_pushundefined(J);
}

static void
mjs_location_assign(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	const char *url;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	url = js_tostring(J, 1);

	if (!url) {
		js_error(J, "out of memory");
		return;
	}
	location_goto_const(vs->doc_view, url, 0);
	js_pushundefined(J);
}

static void
mjs_location_reload(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	location_goto_const(vs->doc_view, struri(vs->doc_view->document->uri), 1);
	js_pushundefined(J);
}

static void
mjs_location_replace(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	const char *url;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	url = js_tostring(J, 1);

	if (!url) {
		js_error(J, "out of memory");
		return;
	}
	struct session *ses = vs->doc_view->session;

	if (ses) {
		struct location *loc = cur_loc(ses);

		if (loc) {
			del_from_history(&ses->history, loc);
		}
	}
	location_goto_const(vs->doc_view, url, 0);
	js_pushundefined(J);
}

/* @location_funcs{"toString"}, @location_funcs{"toLocaleString"} */
static void
mjs_location_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_location_get_property_href(J);
}

int
mjs_location_init(js_State *J)
{
	js_newobject(J);
	{
		addmethod(J, "assign", mjs_location_assign, 1);
		addmethod(J, "reload", mjs_location_reload, 0);
		addmethod(J, "replace", mjs_location_replace, 1);
		addmethod(J, "toString", mjs_location_toString, 0);
		addmethod(J, "toLocaleString", mjs_location_toString, 0);

		addproperty(J, "hash", mjs_location_get_property_hash, mjs_location_set_property_hash);
		addproperty(J, "host", mjs_location_get_property_host, mjs_location_set_property_host);
		addproperty(J, "hostname", mjs_location_get_property_hostname, mjs_location_set_property_hostname);
		addproperty(J, "href", mjs_location_get_property_href, mjs_location_set_property_href);
		addproperty(J, "origin", mjs_location_get_property_origin, NULL);
		addproperty(J, "pathname", mjs_location_get_property_pathname, mjs_location_set_property_pathname);
		addproperty(J, "port", mjs_location_get_property_port, mjs_location_set_property_port);
		addproperty(J, "protocol", mjs_location_get_property_protocol, mjs_location_set_property_protocol);
		addproperty(J, "search", mjs_location_get_property_search, mjs_location_set_property_search);
	}

	return 0;
}

void
mjs_push_location(js_State *J)
{
	mjs_location_init(J);
}
