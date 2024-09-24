#ifndef EL__JS_MUJS_FORM_H
#define EL__JS_MUJS_FORM_H

#include <mujs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct form;

void mjs_push_form_object(js_State *J, struct form *form);

#ifdef __cplusplus
}
#endif

#endif
