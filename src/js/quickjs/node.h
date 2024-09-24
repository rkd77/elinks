#ifndef EL__JS_QUICKJS_NODE_H
#define EL__JS_QUICKJS_NODE_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue js_node_init(JSContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
