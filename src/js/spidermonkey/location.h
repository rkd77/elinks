
#ifndef EL__JS_SPIDERMONKEY_LOCATION_H
#define EL__JS_SPIDERMONKEY_LOCATION_H

#include "js/spidermonkey/util.h"

struct view_state;

extern JSClass location_class;
extern const spidermonkeyFunctionSpec location_funcs[];
extern JSPropertySpec location_props[];
JSObject *getLocation(JSContext *ctx, struct view_state *vs);

#endif
