/* Lua scripting hooks */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "elinks.h"

#include "cache/cache.h"
#include "main/event.h"
#include "protocol/uri.h"
#include "scripting/lua/core.h"
#include "scripting/lua/hooks.h"
#include "session/location.h"
#include "session/session.h"
#include "util/string.h"


/* The events that will trigger the functions below and what they are expected
 * to do is explained in doc/events.txt */

static enum evhook_status
script_hook_goto_url(va_list ap, void *data)
{
	lua_State *L = lua_state;
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	int err;

	if (*url == NULL) return EVENT_HOOK_STATUS_NEXT;

	lua_getglobal(L, "goto_url_hook");
	if (lua_isnil(L, -1)) {
		/* The function is not defined */
		lua_pop(L, 1);
		return EVENT_HOOK_STATUS_NEXT;
	}

	lua_pushstring(L, *url);

	if (!ses || !have_location(ses)) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, struri(cur_loc(ses)->vs.uri));
	}

	if (prepare_lua(ses)) return EVENT_HOOK_STATUS_NEXT;

	err = lua_pcall(L, 2, 1, 0);
	finish_lua();
	if (err) return EVENT_HOOK_STATUS_NEXT;

	if (lua_isstring(L, -1)) {
		unsigned char *new_url;

		new_url = stracpy((unsigned char *) lua_tostring(L, -1));
		if (new_url) {
			mem_free_set(url, new_url);
		}
	} else if (lua_isnil(L, -1)) {
		(*url)[0] = 0;
	} else {
		alert_lua_error("goto_url_hook must return a string or nil");
	}

	lua_pop(L, 1);

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_follow_url(va_list ap, void *data)
{
	lua_State *L = lua_state;
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	int err;

	if (*url == NULL) return EVENT_HOOK_STATUS_NEXT;

	lua_getglobal(L, "follow_url_hook");
	if (lua_isnil(L, -1)) {
		/* The function is not defined */
		lua_pop(L, 1);
		return EVENT_HOOK_STATUS_NEXT;
	}

	lua_pushstring(L, *url);

	if (prepare_lua(ses)) return EVENT_HOOK_STATUS_NEXT;

	err = lua_pcall(L, 1, 1, 0);
	finish_lua();
	if (err) return EVENT_HOOK_STATUS_NEXT;

	if (lua_isstring(L, -1)) {
		unsigned char *new_url;

		new_url = stracpy((unsigned char *) lua_tostring(L, -1));
		if (new_url) {
			mem_free_set(url, new_url);
		}
	} else if (lua_isnil(L, -1)) {
		(*url)[0] = 0;
	} else {
		alert_lua_error("follow_url_hook must return a string or nil");
	}

	lua_pop(L, 1);

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_pre_format_html(va_list ap, void *data)
{
	lua_State *L = lua_state;
	struct session *ses = va_arg(ap, struct session *);
	struct cache_entry *cached = va_arg(ap, struct cache_entry *);
	struct fragment *fragment = get_cache_fragment(cached);
	unsigned char *url = struri(cached->uri);
	int err;

	if (!cached->length || !*fragment->data) return EVENT_HOOK_STATUS_NEXT;

	lua_getglobal(L, "pre_format_html_hook");
	if (lua_isnil(L, -1)) {
		/* The function is not defined */
		lua_pop(L, 1);
		return EVENT_HOOK_STATUS_NEXT;
	}

	lua_pushstring(L, url);
	lua_pushlstring(L, fragment->data, fragment->length);

	if (prepare_lua(ses)) return EVENT_HOOK_STATUS_NEXT;

	err = lua_pcall(L, 2, 1, 0);
	finish_lua();
	if (err) return EVENT_HOOK_STATUS_NEXT;

	if (lua_isstring(L, -1)) {
		int len = lua_strlen(L, -1);

		add_fragment(cached, 0, (unsigned char *) lua_tostring(L, -1), len);
		normalize_cache_entry(cached, len);
	} else if (!lua_isnil(L, -1)) {
		alert_lua_error("pre_format_html_hook must return a string or nil");
	}

	lua_pop(L, 1);

	return EVENT_HOOK_STATUS_NEXT;
}

/* The Lua function can return:
 *  - "PROXY:PORT" to use the specified proxy
 *  - ""           to not use any proxy
 *  - nil          to use the default proxies */
static enum evhook_status
script_hook_get_proxy(va_list ap, void *data)
{
	lua_State *L = lua_state;
	unsigned char **new_proxy_url = va_arg(ap, unsigned char **);
	unsigned char *url = va_arg(ap, unsigned char *);
	int err;

	if (!new_proxy_url || !url)
		return EVENT_HOOK_STATUS_NEXT;

	lua_getglobal(L, "proxy_for_hook");
	if (lua_isnil(L, -1)) {
		/* The function is not defined */
		lua_pop(L, 1);
		return EVENT_HOOK_STATUS_NEXT;
	}

	lua_pushstring(L, url);
	if (prepare_lua(NULL)) return EVENT_HOOK_STATUS_NEXT;

	err = lua_pcall(L, 1, 1, 0);
	finish_lua();
	if (err) return EVENT_HOOK_STATUS_NEXT;

	if (lua_isstring(L, -1)) {
		mem_free_set(new_proxy_url,
		             stracpy((unsigned char *) lua_tostring(L, -1)));
	} else if (lua_isnil(L, -1)) {
		mem_free_set(new_proxy_url, NULL);
	} else {
		alert_lua_error("proxy_hook must return a string or nil");
	}

	lua_pop(L, 1);

	return EVENT_HOOK_STATUS_NEXT;
}

static enum evhook_status
script_hook_quit(va_list ap, void *data)
{
	if (!prepare_lua(NULL)) {
		(void)luaL_dostring(lua_state, "if quit_hook then quit_hook() end");
		finish_lua();
	}

	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info lua_scripting_hooks[] = {
	{ "goto-url", 0, script_hook_goto_url, NULL },
	{ "follow-url", 0, script_hook_follow_url, NULL },
	{ "pre-format-html", 0, script_hook_pre_format_html, NULL },
	{ "get-proxy", 0, script_hook_get_proxy, NULL },
	{ "quit", 0, script_hook_quit, NULL },
	{ "dialog-lua-console", 0, dialog_lua_console, NULL },
	{ "free-history", 0, free_lua_console_history, NULL },
	NULL_EVENT_HOOK_INFO,
};
