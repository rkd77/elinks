#ifndef EL__ECMASCRIPT_SPIDERMONKEY_MESSAGE_H
#define EL__ECMASCRIPT_SPIDERMONKEY_MESSAGE_H

#include "js/spidermonkey/util.h"

extern JSClass messageEvent_class;
extern JSPropertySpec messageEvent_props[];
extern const spidermonkeyFunctionSpec messageEvent_funcs[];

bool messageEvent_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

JSObject *get_messageEvent(JSContext *ctx, char *data, char *origin, char *source);

#endif
