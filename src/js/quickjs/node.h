#ifndef EL__JS_QUICKJS_NODE_H
#define EL__JS_QUICKJS_NODE_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

int js_node_init(JSContext *ctx);
JSValue getNode(JSContext *ctx, void *n);

#ifdef __cplusplus
}
#endif

#endif
