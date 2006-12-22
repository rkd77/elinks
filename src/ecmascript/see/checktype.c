/* Check the type of a SEE object so it's safe to cast */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <see/see.h>
#include "ecmascript/see/checktype.h"

void
see_check_class(struct SEE_interpreter *interp,
		const struct SEE_object *object,
		const struct SEE_objectclass *class)
{
	if (object->objectclass != class)
		SEE_error_throw(interp, interp->TypeError,
				"got %s, expected %s",
				object->objectclass->Class,
				class->Class);
}
