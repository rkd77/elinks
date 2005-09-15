/* $Id: core.h,v 1.14.6.1 2005/01/29 00:53:54 jonas Exp $ */

#ifndef EL__SCRIPTING_LUA_CORE_H
#define EL__SCRIPTING_LUA_CORE_H

#include <lua.h>	/* This is standard include. */
#ifdef HAVE_LAUXLIB_H
#include <lauxlib.h>	/* needed for lua_ref, lua_unref */
#endif
#ifdef HAVE_LUA_PCALL
#define lua_call(L,nargs,nresults)     lua_pcall(L,nargs,nresults,0)
#define lua_open(x)    (lua_open)()
#endif
#ifndef LUA_ALERT
#define LUA_ALERT      "alert"
#endif

#include "sched/event.h"
#include "sched/session.h"

extern lua_State *lua_state;

int prepare_lua(struct session *);
void finish_lua(void);

void alert_lua_error(unsigned char *);
void alert_lua_error2(unsigned char *, unsigned char *);

enum evhook_status dialog_lua_console(va_list ap, void *data);
enum evhook_status free_lua_console_history(va_list ap, void *data);

#endif
