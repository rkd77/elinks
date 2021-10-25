
#ifndef EL__ECMASCRIPT_SPIDERMONKEY_ELEMENT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_ELEMENT_H

#include "ecmascript/spidermonkey/util.h"

extern JSClass element_class;
extern JSPropertySpec element_props[];

JSObject *getElement(JSContext *ctx, void *node);
JSObject *getCollection(JSContext *ctx, void *node);

void walk_tree(struct string *buf, void *nod, bool start = true, bool toSortAttrs = false);

#endif
