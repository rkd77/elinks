#ifndef EL__ECMASCRIPT_MUJS_LOCATION_H
#define EL__ECMASCRIPT_MUJS_LOCATION_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

int mjs_location_init(js_State *J);
void mjs_push_location(js_State *J);

#ifdef __cplusplus
}
#endif

#endif
