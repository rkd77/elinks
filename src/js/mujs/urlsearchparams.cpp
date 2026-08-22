/* The MuJS URLSearchParams object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/ecmascript.h"
#include "js/mujs.h"
#include "js/mujs/urlsearchparams.h"
#include "util/qs_parse/qs_parse.h"

#include <vector>
#include <string>

static void
mjs_urlsearchparams_finalizer(js_State *J, void *val)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)val;

	if (map) {
		map->clear();
		delete map;
	}
}

static void
mjs_urlsearchparams_get_property_size(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)js_touserdata(J, 0, "URLSearchParams");

	if (map) {
		js_pushnumber(J, map->size() / 2);
		return;
	}
	js_pushnumber(J, 0);
}

static void
mjs_urlsearchparams_keys(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)js_touserdata(J, 0, "URLSearchParams");

	if (!map) {
		js_pushundefined(J);
		return;
	}
	int j = map->size() / 2;

	js_newarray(J);
	for (int i = 0; i < j; i++) {
		js_pushstring(J, (*map)[i * 2].c_str());
		js_setindex(J, -2, i);
	}
}

static void
mjs_urlsearchparams_values(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)js_touserdata(J, 0, "URLSearchParams");

	if (!map) {
		js_pushundefined(J);
		return;
	}
	int j = map->size() / 2;

	js_newarray(J);
	for (int i = 0; i < j; i++) {
		js_pushstring(J, (*map)[i * 2 + 1].c_str());
		js_setindex(J, -2, i);
	}
}

static void
mjs_urlsearchparams_entries(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)js_touserdata(J, 0, "URLSearchParams");

	if (!map) {
		js_pushundefined(J);
		return;
	}
	int j = map->size() / 2;
	js_newarray(J);

	for (int i = 0; i < j; i++) {
		js_newarray(J);
		js_pushstring(J, (*map)[i * 2].c_str());
		js_setindex(J, -2, 0);
		js_pushstring(J, (*map)[i * 2 + 1].c_str());
		js_setindex(J, -2, 1);
		js_setindex(J, -2, i);
	}
}

static void
mjs_urlsearchparams_toString(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)js_touserdata(J, 0, "URLSearchParams");

	std::string result;
	const char *prepend = "";
	if (map) {
		for (auto it = map->begin(); it != map->end(); it++) {
			result += prepend;
			result += (*it);
			it++;
			std::string value = (*it);
			if (value != "") {
				result += "=";
				result += value;
			}
			prepend = "&";
		}
	}
	js_pushstring(J, result.c_str());
}

static void
mjs_urlsearchparams_delete(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)js_touserdata(J, 0, "URLSearchParams");

	if (!map) {
		js_pushundefined(J);
		return;
	}
	const char *key = js_tostring(J, 1);

	if (key) {
		for (auto it = map->begin(); it != map->end(); it += 2) {
			if ((*it) == key) {
				auto n = it + 1;
				map->erase(n);
				map->erase(it);
				break;
			}
		}
	}
	js_pushundefined(J);
}

static void
mjs_urlsearchparams_has(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)js_touserdata(J, 0, "URLSearchParams");

	if (!map) {
		js_pushundefined(J);
		return;
	}
	const char *key = js_tostring(J, 1);

	if (key) {
		for (auto it = map->begin(); it != map->end(); it += 2) {
			if ((*it) == key) {
				if (js_isundefined(J, 2)) {
					js_pushboolean(J, 1);
					return;
				}
				const char *value = js_tostring(J, 2);

				if (value && *value) {
					if ((*(it + 1)) == value) {
						js_pushboolean(J, 1);
						return;
					}
				}
			}
		}
	}
	js_pushboolean(J, 0);
}

static void
mjs_urlsearchparams_get(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)js_touserdata(J, 0, "URLSearchParams");

	if (!map) {
		js_pushundefined(J);
		return;
	}
	const char *key = js_tostring(J, 1);

	if (key) {
		for (auto it = map->begin(); it != map->end(); it += 2) {
			if ((*it) == key) {
				auto n = it + 1;
				js_pushstring(J, (*n).c_str());
				return;
			}
		}
	}
	js_pushundefined(J);
}

static void
mjs_urlsearchparams_set(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = (std::vector<std::string>*)js_touserdata(J, 0, "URLSearchParams");

	if (!map) {
		js_pushundefined(J);
		return;
	}
	map->push_back(js_tostring(J, 1));
	map->push_back(js_isstring(J, 2) ? js_tostring(J, 2) : "");
	js_pushundefined(J);
}


static void
mjs_urlsearchparams_fun(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
parse_text(std::vector<std::string>* map, const char *str)
{
	ELOG
	if (!str || !*str) {
		return;
	}

	char *kvpairs[1024];
	int i = qs_parse(str, kvpairs, 1024);
	int j;

	for (j = 0; j < i; j++) {
		char *key = kvpairs[j];
		char *value = strchr(key, '=');
		if (value) {
			*value++ = '\0';
		}
		map->push_back(key);
		map->push_back(value);
	}
}

static void
mjs_urlsearchparams_constructor(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	std::vector<std::string> *map = new std::vector<std::string>;

	if (!map) {
		js_error(J, "out of memory");
		return;
	}

	if (js_isarray(J, 1)) {
		int n = js_getlength(J, 1);
		for (int i = 0; i < n; ++i) {
			js_getindex(J, 1, i);

			if (js_isarray(J, -1)) {
				int length = js_getlength(J, -1);

				if (length == 2) {
					js_getindex(J, -1, 0);
					const char *key = js_tostring(J, -1);
					js_pop(J, 1);
					js_getindex(J, -1, 1);
					const char *value = js_tostring(J, -1);
					js_pop(J, 1);

					map->push_back(key);
					map->push_back(value);
				}
			}
			js_pop(J, 1);
		}
	} else if (js_isobject(J, 1)) {
		js_pushiterator(J, 1, 1);

		const char *key;
		while ((key = js_nextiterator(J, -1)) != NULL) {
			js_getproperty(J, -2, key);
			const char *value = js_tostring(J, -1);
			map->push_back(key);
			map->push_back(value);
			js_pop(J, 1);
		}
		js_pop(J, 1);
	} else {
		const char *str = js_tostring(J, 1);

		parse_text(map, str);
	}
	js_newobject(J);
	{
		js_newuserdata(J, "URLSearchParams", map, mjs_urlsearchparams_finalizer);
		addmethod(J, "URLSearchParams.prototype.toString", mjs_urlsearchparams_toString, 0);
		addmethod(J, "URLSearchParams.prototype.delete", mjs_urlsearchparams_delete, 1);
		addmethod(J, "URLSearchParams.prototype.get", mjs_urlsearchparams_get, 2);
		addmethod(J, "URLSearchParams.prototype.has", mjs_urlsearchparams_has, 2);
		addmethod(J, "URLSearchParams.prototype.set", mjs_urlsearchparams_set, 2);
		addmethod(J, "URLSearchParams.prototype.entries", mjs_urlsearchparams_entries, 0);
		addmethod(J, "URLSearchParams.prototype.keys", mjs_urlsearchparams_keys, 0);
		addmethod(J, "URLSearchParams.prototype.values", mjs_urlsearchparams_values, 0);
		addproperty(J, "size",	mjs_urlsearchparams_get_property_size, NULL);
	}
}

int
mjs_urlsearchparams_init(js_State *J)
{
	ELOG
	js_pushglobal(J);
	js_newcconstructor(J, mjs_urlsearchparams_fun, mjs_urlsearchparams_constructor, "URLSearchParams", 0);
	js_defglobal(J, "URLSearchParams", JS_DONTENUM);
	return 0;
}
