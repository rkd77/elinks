#ifndef EL__ECMASCRIPT_ECMASCRIPT_C_H
#define EL__ECMASCRIPT_ECMASCRIPT_C_H

#include "main/module.h"

#ifdef __cplusplus
extern "C" {
#endif

struct document_options;
struct document;
struct ecmascript_interpreter;
struct session;
struct uri;
struct view_state;

int ecmascript_get_interpreter_count(void);
void ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter);
void toggle_ecmascript(struct session *ses);

/* Takes line with the syntax javascript:<ecmascript code>. Activated when user
 * follows a link with this synstax. */
void ecmascript_protocol_handler(struct session *ses, struct uri *uri);
void check_for_snippets(struct view_state *vs, struct document_options *options, struct document *document);
void kill_ecmascript_timeouts(struct document *document);

extern struct module ecmascript_module;

#ifdef __cplusplus
}
#endif

#endif
