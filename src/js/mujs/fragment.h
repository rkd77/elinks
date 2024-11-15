#ifndef EL__JS_MUJS_FRAGMENT_H
#define EL__JS_MUJS_FRAGMENT_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

void *mjs_getprivate_fragment(js_State *J, int idx);
void mjs_push_fragment(js_State *J, void *node);
int mjs_fragment_init(js_State *J);

#ifdef __cplusplus
}
#endif


#endif
