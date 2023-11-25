#ifndef EL__ECMASCRIPT_ECMASCRIPT_C_H
#define EL__ECMASCRIPT_ECMASCRIPT_C_H

#include "main/module.h"

#ifdef __cplusplus
extern "C" {
#endif

struct session;

int ecmascript_get_interpreter_count(void);
void toggle_ecmascript(struct session *ses);

extern struct module ecmascript_module;

#ifdef __cplusplus
}
#endif

#endif
