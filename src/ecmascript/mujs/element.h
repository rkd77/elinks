#ifndef EL__ECMASCRIPT_MUJS_ELEMENT_H
#define EL__ECMASCRIPT_MUJS_ELEMENT_H

#include <mujs.h>

int mjs_element_init(js_State *J);
void mjs_push_element(js_State *J, void *node);
void walk_tree(struct string *buf, void *nod, bool start = true, bool toSortAttrs = false);

#endif
