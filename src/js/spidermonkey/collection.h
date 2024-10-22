#ifndef EL__JS_SPIDERMONKEY_COLLECTION_H
#define EL__JS_SPIDERMONKEY_COLLECTION_H

#include "js/spidermonkey/util.h"

JSObject *getCollection(JSContext *ctx, void *node);
JSObject *getCollection2(JSContext *ctx, void *node);

#endif
