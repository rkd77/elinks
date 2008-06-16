
#ifndef EL__ECMASCRIPT_SPIDERMONKEY_FORM_H
#define EL__ECMASCRIPT_SPIDERMONKEY_FORM_H

#include "ecmascript/spidermonkey/util.h"

struct form_view;

extern const JSClass forms_class;
extern const spidermonkeyFunctionSpec forms_funcs[];
extern const JSPropertySpec forms_props[];

JSObject *get_form_object(JSContext *ctx, JSObject *jsdoc, struct form_view *fv);

#endif
