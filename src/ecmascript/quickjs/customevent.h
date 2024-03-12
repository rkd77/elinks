#ifndef EL__ECMASCRIPT_QUICKJS_CUSTOMEVENT_H
#define EL__ECMASCRIPT_QUICKJS_CUSTOMEVENT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

int js_customEvent_init(JSContext *ctx);
//JSValue get_keyboardEvent(JSContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
