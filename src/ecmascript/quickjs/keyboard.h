#ifndef EL__ECMASCRIPT_QUICKJS_KEYBOARD_H
#define EL__ECMASCRIPT_QUICKJS_KEYBOARD_H

#include <quickjs/quickjs.h>

struct term_event;

JSValue get_keyboardEvent(JSContext *ctx, struct term_event *ev);

#endif
