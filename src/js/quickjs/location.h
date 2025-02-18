#ifndef EL__JS_QUICKJS_LOCATION_H
#define EL__JS_QUICKJS_LOCATION_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct view_state;

JSValue js_location_init(JSContext *ctx);
JSValue getLocation(JSContext *ctx, struct view_state *vs);

#ifdef __cplusplus
}
#endif

#endif
