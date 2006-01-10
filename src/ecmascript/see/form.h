#ifndef EL__ECMASCRIPT_SEE_FORM_H
#define EL__ECMASCRIPT_SEE_FORM_H

struct js_document_object;
struct ecmascript_interpreter;
struct form_view;

struct js_form {
	struct SEE_object object;
	struct js_document_object *parent;
	struct form_view *fv;
	struct SEE_object *reset;
	struct SEE_object *submit;
};

struct js_form *js_get_form_object(struct SEE_interpreter *, struct js_document_object*, struct form_view *);
void init_js_forms_object(struct ecmascript_interpreter *);

#endif
