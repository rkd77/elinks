#ifndef EL__JS_QUICKJS_ATTR_H
#define EL__JS_QUICKJS_ATTR_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getAttr(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
