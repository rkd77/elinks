#ifndef EL__JS_QUICKJS_TOKENLIST_H
#define EL__JS_QUICKJS_TOKENLIST_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getTokenlist(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
