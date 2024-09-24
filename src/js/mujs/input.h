#ifndef EL__JS_MUJS_INPUT_H
#define EL__JS_MUJS_INPUT_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct form_state;

void mjs_push_input_object(js_State *J, struct form_state *fs);

#ifdef __cplusplus
}
#endif

#endif
