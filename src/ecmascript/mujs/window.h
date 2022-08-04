#ifndef EL__ECMASCRIPT_MUJS_WINDOW_H
#define EL__ECMASCRIPT_MUJS_WINDOW_H

#include <mujs.h>

struct ecmascript_interpreter;

int mjs_window_init(struct ecmascript_interpreter *interpreter, js_State *J);

#endif
