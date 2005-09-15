/* $Id: hooks.h,v 1.11 2004/04/29 23:32:19 jonas Exp $ */

#ifndef EL__SCRIPTING_LUA_HOOKS_H
#define EL__SCRIPTING_LUA_HOOKS_H

#ifdef CONFIG_LUA

#include "sched/event.h"

extern struct event_hook_info lua_scripting_hooks[];

#endif

#endif
