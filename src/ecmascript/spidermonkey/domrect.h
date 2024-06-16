#ifndef EL__ECMASCRIPT_SPIDERMONKEY_DOMRECT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_DOMRECT_H

#include "ecmascript/spidermonkey/util.h"

extern JSClass domRect_class;
extern JSPropertySpec domRect_props[];
JSObject *getDomRect(JSContext *ctx);

#endif
