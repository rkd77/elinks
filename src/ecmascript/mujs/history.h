#ifndef EL__ECMASCRIPT_MUJS_HISTORY_H
#define EL__ECMASCRIPT_MUJS_HISTORY_H

#include <mujs.h>

struct ecmascript_interpreter;

int mjs_history_init(struct ecmascript_interpreter *interpreter, js_State *J);

#endif
