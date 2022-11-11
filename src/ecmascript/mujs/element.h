#ifndef EL__ECMASCRIPT_MUJS_ELEMENT_H
#define EL__ECMASCRIPT_MUJS_ELEMENT_H

#include <mujs.h>

struct term_event;

int mjs_element_init(js_State *J);
void mjs_push_element(js_State *J, void *node);
void walk_tree(struct string *buf, void *nod, bool start = true, bool toSortAttrs = false);
void check_element_event(void *elem, const char *event_name, struct term_event *ev);


#endif
