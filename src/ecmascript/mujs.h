#ifndef EL__ECMASCRIPT_MUJS_H
#define EL__ECMASCRIPT_MUJS_H

#include <musj.h>

struct ecmascript_interpreter;
struct form_view;
struct form_state;
struct string;

void *mujs_get_interpreter(struct ecmascript_interpreter *interpreter);
void mujs_put_interpreter(struct ecmascript_interpreter *interpreter);

void mujs_detach_form_view(struct form_view *fv);
void mujs_detach_form_state(struct form_state *fs);
void mujs_moved_form_state(struct form_state *fs);

void mujs_eval(struct ecmascript_interpreter *interpreter, struct string *code, struct string *ret);
char *mujs_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);
int mujs_eval_boolback(struct ecmascript_interpreter *interpreter, struct string *code);

void mujs_call_function(struct ecmascript_interpreter *interpreter);//, JS::HandleValue fun, struct string *ret);

void free_document(void *doc);

extern struct module mujs_module;
#endif
