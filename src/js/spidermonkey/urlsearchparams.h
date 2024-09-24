#ifndef EL__ECMASCRIPT_SPIDERMONKEY_URLSEARCHPARAMS_H
#define EL__ECMASCRIPT_SPIDERMONKEY_URLSEARCHPARAMS_H

#include "js/spidermonkey/util.h"

extern JSClass urlSearchParams_class;
extern JSPropertySpec urlSearchParams_props[];
extern const spidermonkeyFunctionSpec urlSearchParams_funcs[];
bool urlSearchParams_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

#endif
