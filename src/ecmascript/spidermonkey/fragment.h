#ifndef EL__ECMASCRIPT_SPIDERMONKEY_FRAGMENT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_FRAGMENT_H

#include "ecmascript/spidermonkey/util.h"

extern JSClass fragment_class;
extern JSPropertySpec fragment_props[];

JSObject *getDocumentFragment(JSContext *ctx, void *node);

#endif
