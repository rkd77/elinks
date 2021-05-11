
#ifndef EL__ECMASCRIPT_SPIDERMONKEY_ELEMENT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_ELEMENT_H

#include "ecmascript/spidermonkey/util.h"
#include <htmlcxx/html/ParserDom.h>
using namespace htmlcxx;

extern JSClass element_class;
extern JSPropertySpec element_props[];

JSObject *getElement(JSContext *ctx, void *node);
JSObject *getCollection(JSContext *ctx, void *node);

#endif
