#ifndef EL__JS_QUICKJS_FRAGMENT_H
#define EL__JS_QUICKJS_FRAGMENT_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

extern JSClassID js_fragment_class_id;
void *js_getopaque_fragment(JSValueConst obj, JSClassID class_id);
void *fragment_get_node(JSValueConst obj);

int js_fragment_init(JSContext *ctx);

JSValue getDocumentFragment(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
