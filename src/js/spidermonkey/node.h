#ifndef EL__JS_SPIDERMONKEY_NODE_H
#define EL__JS_SPIDERMONKEY_NODE_H

#include "js/spidermonkey/util.h"

extern JSClass node_class;
extern JSPropertySpec node_static_props[];
bool node_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);
JSObject *getNode(JSContext *ctx, void *node);

#endif
