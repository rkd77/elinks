#ifndef EL__JS_SPIDERMONKEY_ELEMENT_H
#define EL__JS_SPIDERMONKEY_ELEMENT_H

#include "js/spidermonkey/util.h"

struct term_event;
extern JSClass element_class;
extern JSPropertySpec element_props[];
extern const spidermonkeyFunctionSpec element_funcs[];
bool Element_constructor(JSContext* ctx, unsigned argc, JS::Value* vp);

JSObject *getElement(JSContext *ctx, void *node);

void walk_tree(struct string *buf, void *nod, bool start = true, bool toSortAttrs = false);

void check_element_event(void *interpreter, void *elem, const char *event_name, struct term_event *ev);
#endif
