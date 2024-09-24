#ifndef EL__JS_QUICKJS_DOCUMENT_H
#define EL__JS_QUICKJS_DOCUMENT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

void *js_doc_getopaque(JSValueConst obj);
void *document_get_node(JSValueConst obj);
JSValue getDocument(JSContext *ctx, void *doc);
JSValue getDocument2(JSContext *ctx, void *doc);

#ifdef __cplusplus
}
#endif

#endif
