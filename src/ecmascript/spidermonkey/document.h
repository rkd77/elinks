
#ifndef EL__ECMASCRIPT_SPIDERMONKEY_DOCUMENT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_DOCUMENT_H

#include "ecmascript/spidermonkey/util.h"

extern JSClass document_class;
extern const spidermonkeyFunctionSpec document_funcs[];
extern JSPropertySpec document_props[];

JSObject *getDocument(JSContext *ctx, void *doc);

#endif
