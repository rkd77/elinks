
#ifndef EL__JS_SPIDERMONKEY_DOCUMENT_H
#define EL__JS_SPIDERMONKEY_DOCUMENT_H

#include "js/spidermonkey/util.h"

struct ecmascript_interpreter;

extern JSClass document_class;
extern const spidermonkeyFunctionSpec document_funcs[];
extern JSPropertySpec document_props[];

JSObject *getDocument(JSContext *ctx, void *doc);
bool initDocument(JSContext *ctx, struct ecmascript_interpreter *interpreter, JSObject *document_obj, void *doc);

#endif
