#ifndef EL__JS_QUICKJS_DOMRECT_H
#define EL__JS_QUICKJS_DOMRECT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getDomRect(JSContext *ctx);
extern JSClassID js_domRect_class_id;

#ifdef __cplusplus
}
#endif

#endif
