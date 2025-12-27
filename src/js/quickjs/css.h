#ifndef EL__JS_QUICKJS_CSS_H
#define EL__JS_QUICKJS_CSS_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getCSSStyleDeclaration(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
