#ifndef EL__ECMASCRIPT_SPIDERMONKEY_INPUT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_INPUT_H

#include "ecmascript/spidermonkey/util.h"

struct form_state;

JSObject *get_input_object(JSContext *ctx, struct form_state *fs);

#endif
