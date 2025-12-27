#ifndef EL__JS_QUICKJS_DOMRECT_H
#define EL__JS_QUICKJS_DOMRECT_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getDomRect(JSContext *ctx, int x, int y, int width, int height, int top, int right, int bottom, int left);
extern JSClassID js_domRect_class_id;

#ifdef __cplusplus
}
#endif

#endif
