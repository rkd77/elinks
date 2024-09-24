#ifndef EL__JS_SPIDERMONKEY_FORMS_H
#define EL__JS_SPIDERMONKEY_FORMS_H

#include "js/spidermonkey/util.h"

struct form;

extern JSClass forms_class;
extern const spidermonkeyFunctionSpec forms_funcs[];
extern JSPropertySpec forms_props[];

JSObject *get_form_object(JSContext *ctx, JSObject *jsdoc, struct form *form);
JSObject *getForms(JSContext *ctx, void *node);

JSString *unicode_to_jsstring(JSContext *ctx, unicode_val_T u);

#endif
