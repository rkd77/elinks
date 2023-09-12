#ifndef EL__ECMASCRIPT_MUJS_KEYBOARD_H
#define EL__ECMASCRIPT_MUJS_KEYBOARD_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct term_event;

void mjs_push_keyboardEvent(js_State *J, struct term_event *ev);

#ifdef __cplusplus
}
#endif

#endif
