#ifndef EL__ECMASCRIPT_QUICKJS_CSS_H
#define EL__ECMASCRIPT_QUICKJS_CSS_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getCSSStyleDeclaration(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
