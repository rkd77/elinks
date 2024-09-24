#ifndef EL__ECMASCRIPT_QUICKJS_STYLE_H
#define EL__ECMASCRIPT_QUICKJS_STYLE_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getStyle(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
