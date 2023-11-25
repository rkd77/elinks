/* ECMAScript helper functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ecmascript/ecmascript-c.h"

extern int interpreter_count;

int
ecmascript_get_interpreter_count(void)
{
	return interpreter_count;
}
