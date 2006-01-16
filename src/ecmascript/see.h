#ifndef EL__ECMASCRIPT_SEE_H
#define EL__ECMASCRIPT_SEE_H

struct ecmascript_interpreter;
struct string;

void see_init();
void see_done();

void *see_get_interpreter(struct ecmascript_interpreter *interpreter);
void see_put_interpreter(struct ecmascript_interpreter *interpreter);

void see_eval(struct ecmascript_interpreter *interpreter, struct string *code);
unsigned char *see_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);
int see_eval_boolback(struct ecmascript_interpreter *interpreter, struct string *code);

extern struct module see_module;
#endif
