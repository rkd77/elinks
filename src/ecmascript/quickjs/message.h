#ifndef EL__ECMASCRIPT_QUICKJS_MESSAGE_H
#define EL__ECMASCRIPT_QUICKJS_MESSAGE_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue get_messageEvent(JSContext *ctx, char *data, char *origin, char *source);

#ifdef __cplusplus
}
#endif

#endif
