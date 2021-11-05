#ifndef EL__ECMASCRIPT_QUICKJS_DOCUMENT_H
#define EL__ECMASCRIPT_QUICKJS_DOCUMENT_H

#include <quickjs/quickjs.h>

JSValue getDocument(JSContext *ctx, void *doc);
JSValue js_document_init(JSContext *ctx, JSValue global_obj);

#endif
