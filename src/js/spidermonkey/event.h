#ifndef EL__ECMASCRIPT_SPIDERMONKEY_EVENT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_EVENT_H

#include "js/spidermonkey/util.h"

extern JSClass event_class;
extern JSPropertySpec event_props[];
extern const spidermonkeyFunctionSpec event_funcs[];
bool event_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);
JSObject *getEvent(JSContext *ctx, void *eve);

#endif
