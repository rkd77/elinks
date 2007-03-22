/* Lua module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "scripting/lua/core.h"
#include "scripting/lua/hooks.h"


struct module lua_scripting_module = struct_module(
	/* name: */		N_("Lua"),
	/* options: */		NULL,
	/* hooks: */		lua_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_lua,
	/* done: */		cleanup_lua
);
