/* $Id: form.h,v 1.1 2005/09/13 22:12:48 pasky Exp $ */

#ifndef EL__ECMASCRIPT_SPIDERMONKEY_FORM_H
#define EL__ECMASCRIPT_SPIDERMONKEY_FORM_H

#include "ecmascript/spidermonkey/util.h"

struct form_view;

extern const JSClass forms_class;
extern const JSFunctionSpec forms_funcs[];
extern const JSPropertySpec forms_props[];

JSObject *get_form_object(JSContext *ctx, JSObject *jsdoc, struct form_view *fv);

#endif
