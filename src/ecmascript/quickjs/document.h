#ifndef EL__ECMASCRIPT_QUICKJS_DOCUMENT_H
#define EL__ECMASCRIPT_QUICKJS_DOCUMENT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

void *js_doc_getopaque(JSValueConst obj);
JSValue getDocument(JSContext *ctx, void *doc);
JSValue getDocument2(JSContext *ctx, void *doc);

#ifdef __cplusplus
}
#endif

#endif
