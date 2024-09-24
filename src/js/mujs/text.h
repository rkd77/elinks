#ifndef EL__JS_MUJS_TEXT_H
#define EL__JS_MUJS_TEXT_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

void *mjs_getprivate_text(js_State *J, int idx);
void mjs_push_text(js_State *J, void *node);

#ifdef __cplusplus
}
#endif


#endif
