#ifndef EL__SCRIPTING_SMJS_SESSION_OBJECT_H
#define EL__SCRIPTING_SMJS_SESSION_OBJECT_H

#include "ecmascript/spidermonkey-shared.h"

struct session;
struct terminal;

JSObject *smjs_get_session_object(struct session *ses);
JSObject *smjs_get_session_array_object(struct terminal *term);

void smjs_detach_session_array_object(struct terminal *term);

void smjs_init_session_interface(void);

#endif
