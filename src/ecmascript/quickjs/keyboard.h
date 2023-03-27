#ifndef EL__ECMASCRIPT_QUICKJS_KEYBOARD_H
#define EL__ECMASCRIPT_QUICKJS_KEYBOARD_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct term_event;

JSValue get_keyboardEvent(JSContext *ctx, struct term_event *ev);

#ifdef __cplusplus
}
#endif

#endif
