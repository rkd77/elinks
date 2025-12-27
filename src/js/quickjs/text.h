#ifndef EL__JS_QUICKJS_TEXT_H
#define EL__JS_QUICKJS_TEXT_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

extern JSClassID js_text_class_id;
void *js_getopaque_text(JSValueConst obj, JSClassID class_id);
void *text_get_node(JSValueConst obj);
int js_text_init(JSContext *ctx);

JSValue getText(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
