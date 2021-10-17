
#ifndef EL__ECMASCRIPT_QUICKJS_WINDOW_H
#define EL__ECMASCRIPT_QUICKJS_WINDOW_H

#include <quickjs/quickjs.h>

JSValue js_window_alert(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv);

#endif
