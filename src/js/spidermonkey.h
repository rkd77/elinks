#ifndef EL__JS_SPIDERMONKEY_H
#define EL__JS_SPIDERMONKEY_H

#include <jsapi.h>
#include <map>

struct ecmascript_interpreter;
struct form_view;
struct form_state;
struct string;
struct term_event;

void *spidermonkey_get_interpreter(struct ecmascript_interpreter *interpreter);
void spidermonkey_put_interpreter(struct ecmascript_interpreter *interpreter);

void spidermonkey_detach_form_view(struct form_view *fv);
void spidermonkey_detach_form_state(struct form_state *fs);
void spidermonkey_moved_form_state(struct form_state *fs);

void spidermonkey_eval(struct ecmascript_interpreter *interpreter, struct string *code, struct string *ret);
char *spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);
int spidermonkey_eval_boolback(struct ecmascript_interpreter *interpreter, struct string *code);

void spidermonkey_call_function(struct ecmascript_interpreter *interpreter, JS::HandleValue fun, struct string *ret);
void spidermonkey_call_function_timestamp(struct ecmascript_interpreter *interpreter, JS::HandleValue fun, struct string *ret);

int spidermonkey_document_fire_onkeydown(struct ecmascript_interpreter *interpreter, struct term_event *ev);
int spidermonkey_document_fire_onkeyup(struct ecmascript_interpreter *interpreter, struct term_event *ev);

extern struct module spidermonkey_module;

extern std::map<void *, bool> interps;

#endif
