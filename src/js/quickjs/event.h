#ifndef EL__JS_QUICKJS_EVENT_H
#define EL__JS_QUICKJS_EVENT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

int js_event_init(JSContext *ctx);
JSValue getEvent(JSContext *ctx, void *eve);
extern JSClassID js_event_class_id;

#ifdef __cplusplus
}
#endif

#endif
