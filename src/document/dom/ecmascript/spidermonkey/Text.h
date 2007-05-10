#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_TEXT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_TEXT_H

#include "ecmascript/spidermonkey/util.h"

extern const JSClass Text_class;
extern const JSFunctionSpec Text_funcs[];
extern const JSPropertySpec Text_props[];

JSBool Text_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

#endif

