#ifndef EL__JS_MUJS_LOCATION_H
#define EL__JS_MUJS_LOCATION_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct view_state;

int mjs_location_init(js_State *J, struct view_state *vs);
void mjs_push_location(js_State *J, struct view_state *vs);

#ifdef __cplusplus
}
#endif

#endif
