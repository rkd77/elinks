#ifndef EL__ECMASCRIPT_MUJS_LOCALSTORAGE_H
#define EL__ECMASCRIPT_MUJS_LOCALSTORAGE_H

#include <mujs.h>

struct ecmascript_interpreter;

int mjs_localstorage_init(struct ecmascript_interpreter *interpreter, js_State *J);

#endif
