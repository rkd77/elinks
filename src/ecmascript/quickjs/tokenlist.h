#ifndef EL__ECMASCRIPT_QUICKJS_TOKENLIST_H
#define EL__ECMASCRIPT_QUICKJS_TOKENLIST_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getTokenlist(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
