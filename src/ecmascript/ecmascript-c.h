#ifndef EL__ECMASCRIPT_ECMASCRIPT_C_H
#define EL__ECMASCRIPT_ECMASCRIPT_C_H

#include "main/module.h"

#ifdef __cplusplus
extern "C" {
#endif

struct session;
struct uri;

int ecmascript_get_interpreter_count(void);
void toggle_ecmascript(struct session *ses);

/* Takes line with the syntax javascript:<ecmascript code>. Activated when user
 * follows a link with this synstax. */
void ecmascript_protocol_handler(struct session *ses, struct uri *uri);

extern struct module ecmascript_module;

#ifdef __cplusplus
}
#endif

#endif
