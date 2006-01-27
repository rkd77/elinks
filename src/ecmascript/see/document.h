#ifndef EL__ECMASCRIPT_SEE_DOCUMENT_H
#define EL__ECMASCRIPT_SEE_DOCUMENT_H

struct ecmascript_interpreter;

struct js_document_object {
	struct SEE_object object;
	struct SEE_object *write;
	struct SEE_object *writeln;
	struct SEE_object *forms;
};

void init_js_document_object(struct ecmascript_interpreter *);

#endif
