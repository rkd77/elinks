#ifndef EL__ECMASCRIPT_SPIDERMONKEY_XHR_H
#define EL__ECMASCRIPT_SPIDERMONKEY_XHR_H

#include "ecmascript/spidermonkey/util.h"

extern JSClass xhr_class;
extern JSPropertySpec xhr_props[];
extern const spidermonkeyFunctionSpec xhr_funcs[];
bool xhr_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

#endif
