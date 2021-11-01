#ifndef EL__ECMASCRIPT_QUICKJS_FORMS_H
#define EL__ECMASCRIPT_QUICKJS_FORMS_H

#include <quickjs/quickjs.h>

struct form;

JSValue js_get_form_object(JSContext *ctx, JSValue jsdoc, struct form *form);
JSValue getForms(JSContext *ctx, void *node);

//JSString *unicode_to_jsstring(JSContext *ctx, unicode_val_T u);

#endif
