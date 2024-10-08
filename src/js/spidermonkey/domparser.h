#ifndef EL__JS_SPIDERMONKEY_DOMPARSER_H
#define EL__JS_SPIDERMONKEY_DOMPARSER_H

#include "js/spidermonkey/util.h"

extern JSClass domparser_class;
extern const spidermonkeyFunctionSpec domparser_funcs[];
bool domparser_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

#endif
