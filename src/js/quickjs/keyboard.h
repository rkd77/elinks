#ifndef EL__JS_QUICKJS_KEYBOARD_H
#define EL__JS_QUICKJS_KEYBOARD_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct term_event;

int js_keyboardEvent_init(JSContext *ctx);

JSValue get_keyboardEvent(JSContext *ctx, struct term_event *ev);

#ifdef __cplusplus
}
#endif

#endif
