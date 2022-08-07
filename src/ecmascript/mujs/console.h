#ifndef EL__ECMASCRIPT_MUJS_CONSOLE_H
#define EL__ECMASCRIPT_MUJS_CONSOLE_H

#include <mujs.h>

struct ecmascript_interpreter;

int mjs_console_init(struct ecmascript_interpreter *interpreter, js_State *J);

#endif
