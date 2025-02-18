#ifndef EL__JS_MUJS_WINDOW_H
#define EL__JS_MUJS_WINDOW_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct view_state;

int mjs_window_init(js_State *J);
void mjs_push_window(js_State *J, struct view_state *vs);

#ifdef __cplusplus
}
#endif

#endif
