#ifndef EL__JS_MUJS_EVENT_H
#define EL__JS_MUJS_EVENT_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

int mjs_event_init(js_State *J);
void mjs_push_event(js_State *J, void *eve);

#ifdef __cplusplus
}
#endif

#endif
