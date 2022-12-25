#ifndef EL__ECMASCRIPT_SPIDERMONKEY_MESSAGE_H
#define EL__ECMASCRIPT_SPIDERMONKEY_MESSAGE_H

#include "ecmascript/spidermonkey/util.h"

extern JSClass messageEvent_class;
extern JSPropertySpec messageEvent_props[];
bool messageEvent_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

JSObject *get_messageEvent(JSContext *ctx, char *data, char *origin, char *source);

#endif
