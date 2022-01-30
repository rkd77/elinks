#ifndef EL__ECMASCRIPT_QUICKJS_ELEMENT_H
#define EL__ECMASCRIPT_QUICKJS_ELEMENT_H

#include <quickjs/quickjs.h>

JSValue getElement(JSContext *ctx, void *node);

int js_element_init(JSContext *ctx);
void walk_tree(struct string *buf, void *nod, bool start = true, bool toSortAttrs = false);

#endif
