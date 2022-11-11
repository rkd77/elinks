#ifndef EL__ECMASCRIPT_QUICKJS_ELEMENT_H
#define EL__ECMASCRIPT_QUICKJS_ELEMENT_H

#include <quickjs/quickjs.h>

struct term_event;

JSValue getElement(JSContext *ctx, void *node);

int js_element_init(JSContext *ctx);
void walk_tree(struct string *buf, void *nod, bool start = true, bool toSortAttrs = false);
void check_element_event(void *elem, const char *event_name, struct term_event *ev);

#endif
