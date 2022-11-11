#ifndef EL__ECMASCRIPT_MUJS_KEYBOARD_H
#define EL__ECMASCRIPT_MUJS_KEYBAORD_H

#include <mujs.h>

struct term_event;

void mjs_push_keyboardEvent(js_State *J, struct term_event *ev);

#endif
