
#ifndef EL__SCRIPTING_LUA_CORE_H
#define EL__SCRIPTING_LUA_CORE_H

#include <lua.h>	/* This is standard include. */
#ifdef HAVE_LAUXLIB_H
#include <lauxlib.h>	/* needed for lua_ref, lua_unref */
#endif
#ifndef LUA_ALERT
#define LUA_ALERT      "_ALERT"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct module;
struct session;

extern lua_State *lua_state;

int prepare_lua(struct session *);
void finish_lua(void);

void alert_lua_error(char *);
void alert_lua_error2(char *, char *);

enum evhook_status dialog_lua_console(va_list ap, void *data);
enum evhook_status free_lua_console_history(va_list ap, void *data);

void init_lua(struct module *module);
void cleanup_lua(struct module *module);

#ifdef __cplusplus
}
#endif

#endif
