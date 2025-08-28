#ifndef EL__JS_QUICKJS_H
#define EL__JS_QUICKJS_H

#include <quickjs/quickjs.h>

#if !defined(JS_NAN_BOXING) && defined(__cplusplus)
inline int operator<(JSValueConst a, JSValueConst b)
{
	return JS_VALUE_GET_PTR(a) < JS_VALUE_GET_PTR(b);
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JS_VALUE_GET_STRING
#define JS_VALUE_GET_STRING(v) ((JSString *)JS_VALUE_GET_PTR(v))
#endif

#ifdef ECMASCRIPT_DEBUG

#define RETURN_JS(obj) \
	fprintf(stderr, "%s:%d obj=%p\n", __FILE__, __LINE__, JS_VALUE_GET_PTR(obj)); \
    if (JS_VALUE_HAS_REF_COUNT(obj)) { \
        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(obj); \
        fprintf(stderr, "ref_count=%d\n", p->ref_count); \
    } \
	return obj

#define REF_JS(obj) \
	fprintf(stderr, "%s:%d obj=%p\n", __FILE__, __LINE__, JS_VALUE_GET_PTR(obj)); \
    if (JS_VALUE_HAS_REF_COUNT(obj)) { \
        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(obj); \
        fprintf(stderr, "ref_count=%d\n", p->ref_count); \
    }

#else

#define RETURN_JS(obj) return obj
#define REF_JS(obj)

#endif

struct ecmascript_interpreter;
struct form_view;
struct form_state;
struct string;

void *quickjs_get_interpreter(struct ecmascript_interpreter *interpreter);
void quickjs_put_interpreter(struct ecmascript_interpreter *interpreter);

void quickjs_detach_form_view(struct form_view *fv);
void quickjs_detach_form_state(struct form_state *fs);
void quickjs_moved_form_state(struct form_state *fs);

void quickjs_eval(struct ecmascript_interpreter *interpreter, struct string *code, struct string *ret);
char *quickjs_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);
int quickjs_eval_boolback(struct ecmascript_interpreter *interpreter, struct string *code);

void quickjs_call_function(struct ecmascript_interpreter *interpreter, JSValueConst fun, struct string *ret);
void quickjs_call_function_timestamp(struct ecmascript_interpreter *interpreter, JSValueConst fun, struct string *ret);

extern void *interps;
extern struct module quickjs_module;

#ifdef __cplusplus
}
#endif

#endif
