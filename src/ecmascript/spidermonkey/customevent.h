#ifndef EL__ECMASCRIPT_SPIDERMONKEY_CUSTOMEVENT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_CUSTOMEVENT_H

#include "ecmascript/spidermonkey/util.h"

extern JSClass customEvent_class;
extern JSPropertySpec customEvent_props[];
extern const spidermonkeyFunctionSpec customEvent_funcs[];
bool customEvent_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

#endif
