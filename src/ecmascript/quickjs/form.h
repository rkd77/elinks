#ifndef EL__ECMASCRIPT_QUICKJS_FORM_H
#define EL__ECMASCRIPT_QUICKJS_FORM_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct form;

JSValue js_get_form_object(JSContext *ctx, JSValueConst jsdoc, struct form *form);

#ifdef __cplusplus
}
#endif

#endif
