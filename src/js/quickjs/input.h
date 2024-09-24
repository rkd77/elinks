#ifndef EL__JS_QUICKJS_INPUT_H
#define EL__JS_QUICKJS_INPUT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct form_state;

JSValue js_get_input_object(JSContext *ctx, struct form_state *fs);

#ifdef __cplusplus
}
#endif

#endif
