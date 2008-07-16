#ifndef EL__SCRIPTING_SMJS_GLOBAL_OBJECT_H
#define EL__SCRIPTING_SMJS_GLOBAL_OBJECT_H

#include "ecmascript/spidermonkey-shared.h"

/* The root of the object hierarchy. If object 'foo' has this as its parent,
 * you can use foo's method and properties with 'foo.<method|property>'. */
extern JSObject *smjs_global_object;

/* Create the global object and assign it to smjs_global_object. */
void smjs_init_global_object(void);

#endif
