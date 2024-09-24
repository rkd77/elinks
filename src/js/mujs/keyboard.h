#ifndef EL__JS_MUJS_KEYBOARD_H
#define EL__JS_MUJS_KEYBOARD_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct term_event;

void mjs_push_keyboardEvent(js_State *J, struct term_event *ev, const char *type_);
int mjs_keyboardEvent_init(js_State *J);


#ifdef __cplusplus
}
#endif

#endif
