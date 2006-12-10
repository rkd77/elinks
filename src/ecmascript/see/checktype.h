#ifndef EL__ECMASCRIPT_SEE_CHECKTYPE_H
#define EL__ECMASCRIPT_SEE_CHECKTYPE_H

struct SEE_interpreter;
struct SEE_object;
struct SEE_objectclass;

void see_check_class(struct SEE_interpreter *interp,
		     const struct SEE_object *object,
		     const struct SEE_objectclass *class);

#endif
