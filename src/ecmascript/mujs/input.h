#ifndef EL__ECMASCRIPT_MUJS_INPUT_H
#define EL__ECMASCRIPT_MUJS_INPUT_H

#include <mujs.h>

struct form_state;

void mjs_push_input_object(js_State *J, struct form_state *fs);

#endif
