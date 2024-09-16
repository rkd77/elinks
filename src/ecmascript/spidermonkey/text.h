#ifndef EL__ECMASCRIPT_SPIDERMONKEY_TEXT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_TEXT_H

#include "ecmascript/spidermonkey/util.h"

extern JSClass text_class;
extern JSPropertySpec text_props[];

JSObject *getText(JSContext *ctx, void *node);

#endif
