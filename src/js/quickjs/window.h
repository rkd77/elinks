#ifndef EL__JS_QUICKJS_WINDOW_H
#define EL__JS_QUICKJS_WINDOW_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

int js_window_init(JSContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
