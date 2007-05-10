#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_NODE_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_NODE_H

#include "ecmascript/spidermonkey/util.h"

extern const JSClass Node_class;
extern const JSFunctionSpec Node_funcs[];
extern const JSPropertySpec Node_props[];

JSBool Node_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
JSBool Node_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

#endif

