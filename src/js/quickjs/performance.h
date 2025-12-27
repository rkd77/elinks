#ifndef EL__JS_QUICKJS_PERFORMANCE_H
#define EL__JS_QUICKJS_PERFORMANCE_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getPerformance(JSContext *ctx);
extern JSClassID js_performance_class_id;

#ifdef __cplusplus
}
#endif

#endif
