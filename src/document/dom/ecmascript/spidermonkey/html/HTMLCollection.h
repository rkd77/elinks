#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLCOLLECTION_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLCOLLECTION_H

#include "ecmascript/spidermonkey/util.h"

extern const JSClass HTMLCollection_class;
extern const JSFunctionSpec HTMLCollection_funcs[];
extern const JSPropertySpec HTMLCollection_props[];

JSBool HTMLCollection_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool HTMLCollection_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

#endif
