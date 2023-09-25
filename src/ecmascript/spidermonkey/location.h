
#ifndef EL__ECMASCRIPT_SPIDERMONKEY_LOCATION_H
#define EL__ECMASCRIPT_SPIDERMONKEY_LOCATION_H

#include "ecmascript/spidermonkey/util.h"

extern JSClass location_class;
extern const spidermonkeyFunctionSpec location_funcs[];
extern JSPropertySpec location_props[];
JSObject *getLocation(JSContext *ctx);


#endif
