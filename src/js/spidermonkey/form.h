#ifndef EL__JS_SPIDERMONKEY_FORM_H
#define EL__JS_SPIDERMONKEY_FORM_H

#include "js/spidermonkey/util.h"

struct form;

extern JSClass form_class;
extern JSClass forms_class;
extern const spidermonkeyFunctionSpec forms_funcs[];
extern JSPropertySpec forms_props[];

JSObject *get_form_object(JSContext *ctx, struct form *form);

#endif
