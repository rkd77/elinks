#ifndef EL__JS_SPIDERMONKEY_WINDOW_H
#define EL__JS_SPIDERMONKEY_WINDOW_H

#include "js/spidermonkey/util.h"

struct view_state;

extern JSClass window_class;
extern JSPropertySpec window_props[];
extern const spidermonkeyFunctionSpec window_funcs[];
JSObject *getWindow(JSContext *ctx, struct view_state *vs);

#endif
