#ifndef EL__ECMASCRIPT_QUICKJS_INPUT_H
#define EL__ECMASCRIPT_QUICKJS_INPUT_H

#include <quickjs/quickjs.h>

struct form_state;

JSValue js_get_input_object(JSContext *ctx, struct form_state *fs);

#endif
