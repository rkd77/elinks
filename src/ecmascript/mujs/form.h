#ifndef EL__ECMASCRIPT_MUJS_FORM_H
#define EL__ECMASCRIPT_MUJS_FORM_H

#include <mujs.h>

struct form;

void mjs_push_form_object(js_State *J, struct form *form);

#endif
