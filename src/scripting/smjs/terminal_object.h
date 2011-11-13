#ifndef EL__SCRIPTING_SMJS_TERMINAL_OBJECT_H
#define EL__SCRIPTING_SMJS_TERMINAL_OBJECT_H

struct terminal;

JSObject *smjs_get_terminal_object(struct terminal *term);

void smjs_init_terminal_interface(void);

#endif
