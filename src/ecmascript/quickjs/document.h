#ifndef EL__ECMASCRIPT_QUICKJS_DOCUMENT_H
#define EL__ECMASCRIPT_QUICKJS_DOCUMENT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getDocument(JSContext *ctx, void *doc);
JSValue js_document_init(JSContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
