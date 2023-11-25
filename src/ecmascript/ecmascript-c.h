#ifndef EL__ECMASCRIPT_ECMASCRIPT_C_H
#define EL__ECMASCRIPT_ECMASCRIPT_C_H

#ifdef __cplusplus
extern "C" {
#endif

struct session;

int ecmascript_get_interpreter_count(void);
void toggle_ecmascript(struct session *ses);

#ifdef __cplusplus
}
#endif

#endif
