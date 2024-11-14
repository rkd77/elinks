#ifndef EL__JS_SPIDERMONKEY_DOCUMENT_H
#define EL__JS_SPIDERMONKEY_DOCUMENT_H

#include "js/spidermonkey/util.h"

struct ecmascript_interpreter;

extern JSClass document_class;
extern const spidermonkeyFunctionSpec document_funcs[];
extern JSPropertySpec document_props[];

JSObject *getDocument(JSContext *ctx, void *doc);
JSObject *getDoctype(JSContext *ctx, void *node);
bool Document_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

bool initDocument(JSContext *ctx, struct ecmascript_interpreter *interpreter, JSObject *document_obj, void *doc);

#endif
