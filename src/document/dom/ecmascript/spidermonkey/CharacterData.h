#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_CHARACTERDATA_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_CHARACTERDATA_H

#include "ecmascript/spidermonkey/util.h"

extern const JSClass CharacterData_class;
extern const JSFunctionSpec CharacterData_funcs[];
extern const JSPropertySpec CharacterData_props[];

JSBool CharacterData_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
JSBool CharacterData_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

#endif

