#ifndef EL__SCRIPTING_SMJS_ELINKS_OBJECT_H
#define EL__SCRIPTING_SMJS_ELINKS_OBJECT_H

#include "ecmascript/spidermonkey/util.h"

JSObject *smjs_get_elinks_object(void);
JSBool smjs_invoke_elinks_object_method(unsigned char *method,
                                        jsval argv[], int argc, jsval *rval);

#endif
