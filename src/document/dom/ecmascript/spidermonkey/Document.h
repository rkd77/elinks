#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_DOCUMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_DOCUMENT_H

#include "ecmascript/spidermonkey/util.h"

extern const JSClass Document_class;
extern const JSFunctionSpec Document_funcs[];
extern const JSPropertySpec Document_props[];

JSBool Document_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
JSBool Document_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

#endif

