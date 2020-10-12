#ifndef EL__SCRIPTING_SMJS_ELINKS_OBJECT_H
#define EL__SCRIPTING_SMJS_ELINKS_OBJECT_H

#include "ecmascript/spidermonkey-shared.h"

/* This is the all-powerful elinks object through which all client scripts
 * will interface with ELinks. */
extern JSObject *smjs_elinks_object;

/* Initialise elinks_object. */
void smjs_init_elinks_object(void);

/* Invoke elinks.<method> with the given arguments and put the return value
 * into *rval. */
bool smjs_invoke_elinks_object_method(unsigned char *method, int argc, jsval *argv, JS::MutableHandleValue rval);

#endif
