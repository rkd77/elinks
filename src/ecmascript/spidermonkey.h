#ifndef EL__ECMASCRIPT_SPIDERMONKEY_H
#define EL__ECMASCRIPT_SPIDERMONKEY_H

struct ecmascript_interpreter;
struct string;

void *spidermonkey_get_interpreter(struct ecmascript_interpreter *interpreter);
void spidermonkey_put_interpreter(struct ecmascript_interpreter *interpreter);

void spidermonkey_detach_form_view(struct form_view *fv);
void spidermonkey_detach_form_state(struct form_state *fs);
void spidermonkey_moved_form_state(struct form_state *fs);

void spidermonkey_eval(struct ecmascript_interpreter *interpreter, struct string *code, struct string *ret);
unsigned char *spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);
int spidermonkey_eval_boolback(struct ecmascript_interpreter *interpreter, struct string *code);

extern struct module spidermonkey_module;
#endif
