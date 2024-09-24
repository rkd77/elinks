#ifndef EL__ECMASCRIPT_QUICKJS_FORMS_H
#define EL__ECMASCRIPT_QUICKJS_FORMS_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct form;

JSValue js_get_form_object(JSContext *ctx, JSValue jsdoc, struct form *form);
JSValue getForms(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
