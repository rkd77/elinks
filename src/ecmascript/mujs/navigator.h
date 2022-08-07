#ifndef EL__ECMASCRIPT_MUJS_NAVIGATOR_H
#define EL__ECMASCRIPT_MUJS_NAVIGATOR_H

#include <mujs.h>

struct ecmascript_interpreter;

int mjs_navigator_init(struct ecmascript_interpreter *interpreter, js_State *J);

#endif
