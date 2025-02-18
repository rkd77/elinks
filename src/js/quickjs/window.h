#ifndef EL__JS_QUICKJS_WINDOW_H
#define EL__JS_QUICKJS_WINDOW_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct view_state;

int js_window_init(JSContext *ctx);
JSValue getWindow(JSContext *ctx, struct view_state *vs);

#ifdef __cplusplus
}
#endif

#endif
