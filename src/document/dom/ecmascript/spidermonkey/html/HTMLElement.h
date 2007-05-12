#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLELEMENT_H

#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLElement_class;
extern const JSFunctionSpec HTMLElement_funcs[];
extern const JSPropertySpec HTMLElement_props[];

JSBool HTMLElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
JSBool HTMLElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

#endif
