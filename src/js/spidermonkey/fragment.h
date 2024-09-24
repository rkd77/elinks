#ifndef EL__JS_SPIDERMONKEY_FRAGMENT_H
#define EL__JS_SPIDERMONKEY_FRAGMENT_H

#include "js/spidermonkey/util.h"

extern JSClass fragment_class;
extern JSPropertySpec fragment_props[];

JSObject *getDocumentFragment(JSContext *ctx, void *node);

#endif
