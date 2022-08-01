#ifndef EL__ECMASCRIPT_MUJS_H
#define EL__ECMASCRIPT_MUJS_H

#include <mujs.h>

#ifdef ECMASCRIPT_DEBUG

#if 0

#define RETURN_JS(obj) \
	fprintf(stderr, "%s:%d obj=%p\n", __FILE__, __LINE__, JS_VALUE_GET_PTR(obj)); \
    if (JS_VALUE_HAS_REF_COUNT(obj)) { \
        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(obj); \
        fprintf(stderr, "ref_count=%d\n", p->ref_count); \
    } \
	return obj

#else

#define RETURN_JS(obj) return obj

#endif

#endif

struct ecmascript_interpreter;
struct form_view;
struct form_state;
struct string;

void *mujs_get_interpreter(struct ecmascript_interpreter *interpreter);
void mujs_put_interpreter(struct ecmascript_interpreter *interpreter);

//void mujs_detach_form_view(struct form_view *fv);
//void mujs_detach_form_state(struct form_state *fs);
//void mujs_moved_form_state(struct form_state *fs);

void mujs_eval(struct ecmascript_interpreter *interpreter, struct string *code, struct string *ret);
char *mujs_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);
int mujs_eval_boolback(struct ecmascript_interpreter *interpreter, struct string *code);

//void mujs_call_function(struct ecmascript_interpreter *interpreter, JSValueConst fun, struct string *ret);

extern struct module mujs_module;
#endif
