#ifndef EL__ECMASCRIPT_QUICKJS_IMPLEMENTATION_H
#define EL__ECMASCRIPT_QUICKJS_IMPLEMENTATION_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getImplementation(JSContext *ctx);

#ifdef __cplusplus
}
#endif

#endif



