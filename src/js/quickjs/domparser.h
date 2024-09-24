#ifndef EL__JS_QUICKJS_DOMPARSER_H
#define EL__JS_QUICKJS_DOMPARSER_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

int js_domparser_init(JSContext *ctx);
extern JSClassID js_domparser_class_id;

#ifdef __cplusplus
}
#endif

#endif
