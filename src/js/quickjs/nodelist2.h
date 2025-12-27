#ifndef EL__JS_QUICKJS_NODELIST2_H
#define EL__JS_QUICKJS_NODELIST2_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getNodeList2(JSContext *ctx, void *nodes);

#ifdef __cplusplus
}
#endif

#endif
