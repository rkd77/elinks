#ifndef EL__ECMASCRIPT_SPIDERMONKEY_URL_H
#define EL__ECMASCRIPT_SPIDERMONKEY_URL_H

#include "js/spidermonkey/util.h"

extern JSClass url_class;
extern JSPropertySpec url_props[];
extern const spidermonkeyFunctionSpec url_funcs[];
bool url_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

#endif
