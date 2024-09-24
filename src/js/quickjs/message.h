#ifndef EL__JS_QUICKJS_MESSAGE_H
#define EL__JS_QUICKJS_MESSAGE_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

int js_messageEvent_init(JSContext *ctx);
JSValue get_messageEvent(JSContext *ctx, char *data, char *origin, char *source);

#ifdef __cplusplus
}
#endif

#endif
