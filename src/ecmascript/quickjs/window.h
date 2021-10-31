#ifndef EL__ECMASCRIPT_QUICKJS_WINDOW_H
#define EL__ECMASCRIPT_QUICKJS_WINDOW_H

#include <quickjs/quickjs.h>

int js_window_init(JSContext *ctx, JSValue global_obj);

#endif
