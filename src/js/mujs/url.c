/* The MuJS URL object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/ecmascript.h"
#include "js/mujs.h"
#include "js/mujs/url.h"
#include "protocol/uri.h"

struct eljs_url {
	struct uri uri;
	char *hash;
	char *host;
	char *pathname;
	char *port;
	char *protocol;
	char *search;
};

static void
mjs_url_finalizer(js_State *J, void *val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)val;

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

static void
mjs_url_get_property_hash(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}

	struct string fragment;
	if (!init_string(&fragment)) {
		js_error(J, "out of memory");
		return;
	}

	if (url->uri.fragmentlen) {
		add_bytes_to_string(&fragment, url->uri.fragment, url->uri.fragmentlen);
	}
	js_pushstring(J, fragment.source);
	done_string(&fragment);
}

static void
mjs_url_get_property_host(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	char *str = get_uri_string(&url->uri, URI_HOST_PORT);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "out of memory");
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_url_get_property_hostname(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	char *str = get_uri_string(&url->uri, URI_HOST);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "out of memory");
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_url_get_property_href(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	char *str = get_uri_string(&url->uri, URI_ORIGINAL);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "out of memory");
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_url_get_property_origin(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	char *str = get_uri_string(&url->uri, URI_SERVER);

	if (!str) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "out of memory");
		return;
	}
	js_pushstring(J, str);
	mem_free(str);
}

static void
mjs_url_get_property_pathname(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	struct string pathname;
	if (!init_string(&pathname)) {
		js_error(J, "out of memory");
		return;
	}
	const char *query = (const char *)memchr(url->uri.data, '?', url->uri.datalen);
	int len = (query ? query - url->uri.data : url->uri.datalen);

	add_char_to_string(&pathname, '/');
	add_bytes_to_string(&pathname, url->uri.data, len);
	js_pushstring(J, pathname.source);
	done_string(&pathname);
}

static void
mjs_url_get_property_port(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	struct string port;
	if (!init_string(&port)) {
		js_error(J, "out of memory");
		return;
	}
	if (url->uri.portlen) {
		add_bytes_to_string(&port, url->uri.port, url->uri.portlen);
	}
	js_pushstring(J, port.source);
	done_string(&port);
}

static void
mjs_url_get_property_protocol(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	struct string proto;
	if (!init_string(&proto)) {
		js_error(J, "out of memory");
		return;
	}

	/* Custom or unknown keep the URI untouched. */
	if (url->uri.protocol == PROTOCOL_UNKNOWN) {
		add_to_string(&proto, struri(&url->uri));
	} else {
		add_bytes_to_string(&proto, url->uri.string, url->uri.protocollen);
		add_char_to_string(&proto, ':');
	}
	js_pushstring(J, proto.source);
	done_string(&proto);
}

static void
mjs_url_get_property_search(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	struct string search;

	if (!init_string(&search)) {
		js_error(J, "out of memory");
		return;
	}
	const char *query = (const char *)memchr(url->uri.data, '?', url->uri.datalen);

	if (query) {
		add_bytes_to_string(&search, query, strcspn(query, "#" POST_CHAR_S));
	}
	js_pushstring(J, search.source);
	done_string(&search);
}

static void
mjs_url_set_property_hash(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	char *hash = stracpy(str);

	mem_free_set(&url->hash, hash);

	if (hash) {
		url->uri.fragment = hash;
		url->uri.fragmentlen = strlen(hash);
	}
	js_pushundefined(J);
}

static void
mjs_url_set_property_host(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	char *host = stracpy(str);

	mem_free_set(&url->host, host);

	if (host) {
		url->uri.host = host;
		url->uri.hostlen = strlen(str);
	}
	js_pushundefined(J);
}

static void
mjs_url_set_property_hostname(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	char *hostname = stracpy(str);

	mem_free_set(&url->host, hostname);

	if (hostname) {
		url->uri.host = hostname;
		url->uri.hostlen = strlen(hostname);
	}
	js_pushundefined(J);
}

static void
mjs_url_set_property_href(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	done_uri(&url->uri);

	char *urlstring = stracpy(str);

	if (!urlstring) {
		js_error(J, "out of memory");
		return;
	}
	int ret = parse_uri(&url->uri, urlstring);

	if (ret != URI_ERRNO_OK) {
		js_error(J, "error");
		return;
	}
	js_pushundefined(J);
}

static void
mjs_url_set_property_pathname(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	char *pathname = stracpy(str);

	mem_free_set(&url->pathname, pathname);

	if (pathname) {
		url->uri.data = pathname;
		url->uri.datalen = strlen(pathname);
	}
	js_pushundefined(J);
}

static void
mjs_url_set_property_port(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	char *port = stracpy(str);

	mem_free_set(&url->port, port);

	if (port) {
		url->uri.port = port;
		url->uri.portlen = strlen(port);
	}
	js_pushundefined(J);
}

static inline int
get_protocol_length(const char *url)
{
	ELOG
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

static void
mjs_url_set_property_protocol(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	char *protocol = stracpy(str);

	mem_free_set(&url->protocol, protocol);

	if (protocol) {
		url->uri.protocollen = get_protocol_length(protocol);
		/* Figure out whether the protocol is known */
		url->uri.protocol = get_protocol(protocol, url->uri.protocollen);
	}
	js_pushundefined(J);
}

static void
mjs_url_set_property_search(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)js_touserdata(J, 0, "URL");

	if (!url) {
		js_pushnull(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	char *search = stracpy(str);

	mem_free_set(&url->search, search);

	if (search) {
		// TODO
	}
	js_pushundefined(J);
}

static void
mjs_url_toString(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_url_get_property_href(J);
}

static void
mjs_url_fun(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
mjs_url_constructor(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_url *url = (struct eljs_url *)mem_calloc(1, sizeof(*url));

	if (!url) {
		js_error(J, "out of memory");
		return;
	}
	const char *urlstring = js_tostring(J, 1);
	char *us = null_or_stracpy(urlstring);

	if (!us) {
		js_error(J, "out of memory");
		return;
	}
	int ret = parse_uri(&url->uri, us);

	if (ret != URI_ERRNO_OK) {
		js_error(J, "error");
		return;
	}
	js_newobject(J);
	{
		js_newuserdata(J, "URL", url, mjs_url_finalizer);
		addmethod(J, "URL.prototype.toString", mjs_url_toString, 0);

		addproperty(J, "hash",	mjs_url_get_property_hash, mjs_url_set_property_hash);
		addproperty(J, "host",	mjs_url_get_property_host, mjs_url_set_property_host);
		addproperty(J, "hostname",	mjs_url_get_property_hostname, mjs_url_set_property_hostname);
		addproperty(J, "href",	mjs_url_get_property_href, mjs_url_set_property_href);
		addproperty(J, "origin",	mjs_url_get_property_origin, NULL);
		addproperty(J, "pathname",	mjs_url_get_property_pathname, mjs_url_set_property_pathname);
		addproperty(J, "port",	mjs_url_get_property_port, mjs_url_set_property_port);
		addproperty(J, "protocol",	mjs_url_get_property_protocol, mjs_url_set_property_protocol);
		addproperty(J, "search",	mjs_url_get_property_search, mjs_url_set_property_search);
	}
}

int
mjs_url_init(js_State *J)
{
	ELOG
	js_pushglobal(J);
	js_newcconstructor(J, mjs_url_fun, mjs_url_constructor, "URL", 0);
	js_defglobal(J, "URL", JS_DONTENUM);
	return 0;
}
