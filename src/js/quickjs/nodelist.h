#ifndef EL__ECMASCRIPT_QUICKJS_NODELIST_H
#define EL__ECMASCRIPT_QUICKJS_NODELIST_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getNodeList(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
