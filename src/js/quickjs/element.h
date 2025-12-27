#ifndef EL__JS_QUICKJS_ELEMENT_H
#define EL__JS_QUICKJS_ELEMENT_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct term_event;

extern JSClassID js_element_class_id;
void *js_getopaque(JSValueConst obj, JSClassID class_id);
void *js_getopaque_any(JSValueConst obj);

JSValue getElement(JSContext *ctx, void *node);

int js_element_init(JSContext *ctx);
void walk_tree(struct string *buf, void *nod, bool start, bool toSortAttrs);
void check_element_event(void *interp, void *elem, const char *event_name, struct term_event *ev);

void unset_el_private(void *priv);

#ifdef __cplusplus
}
#endif

#endif
