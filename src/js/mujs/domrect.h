#ifndef EL__JS_MUJS_DOMRECT_H
#define EL__JS_MUJS_DOMRECT_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

void mjs_push_domRect(js_State *J, int x, int y, int width, int height, int top, int right, int bottom, int left);

#ifdef __cplusplus
}
#endif

#endif
