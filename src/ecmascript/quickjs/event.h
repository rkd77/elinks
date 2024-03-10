#ifndef EL__ECMASCRIPT_QUICKJS_EVENT_H
#define EL__ECMASCRIPT_QUICKJS_EVENT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

int js_event_init(JSContext *ctx);
//JSValue get_keyboardEvent(JSContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
