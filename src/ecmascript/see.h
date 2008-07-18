#ifndef EL__ECMASCRIPT_SEE_H
#define EL__ECMASCRIPT_SEE_H

struct ecmascript_interpreter;
struct string;

void *see_get_interpreter(struct ecmascript_interpreter *interpreter);
void see_put_interpreter(struct ecmascript_interpreter *interpreter);

void see_detach_form_view(struct form_view *fv);
void see_detach_form_state(struct form_state *fs);
void see_moved_form_state(struct form_state *fs);

void see_eval(struct ecmascript_interpreter *interpreter, struct string *code, struct string *ret);
unsigned char *see_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);
int see_eval_boolback(struct ecmascript_interpreter *interpreter, struct string *code);

extern struct module see_module;
#endif
