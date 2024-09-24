#ifndef EL__JS_SPIDERMONKEY_INPUT_H
#define EL__JS_SPIDERMONKEY_INPUT_H

#include "js/spidermonkey/util.h"

struct form_state;

JSObject *get_input_object(JSContext *ctx, struct form_state *fs);

#endif
