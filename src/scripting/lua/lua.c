/* Lua module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/libintl.h"
#include "main/module.h"
#include "scripting/lua/core.h"
#include "scripting/lua/hooks.h"

static const char *
get_name_lua(struct module *xxx)
{
	ELOG
	static char elluaversion[32];
	strncpy(elluaversion, LUA_RELEASE, 31);

	return elluaversion;
}


struct module lua_scripting_module = struct_module(
	/* name: */		N_("Lua"),
	/* options: */		NULL,
	/* hooks: */		lua_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_lua,
	/* done: */		cleanup_lua,
	/* getname: */	get_name_lua
);
