#ifndef EL__ECMASCRIPT_QUICKJS_FRAGMENT_H
#define EL__ECMASCRIPT_QUICKJS_FRAGMENT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

extern JSClassID js_fragment_class_id;
void *js_getopaque_fragment(JSValueConst obj, JSClassID class_id);

JSValue getDocumentFragment(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
