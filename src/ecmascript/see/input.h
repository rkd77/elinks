#ifndef EL__ECMASCRIPT_SEE_INPUT_H
#define EL__ECMASCRIPT_SEE_INPUT_H

#include <see/see.h>

struct SEE_input *SEE_input_elinks(struct SEE_interpreter *, unsigned char *);
unsigned char *SEE_string_to_unsigned_char(struct SEE_string *);
unsigned char *SEE_value_to_unsigned_char(struct SEE_interpreter *, struct SEE_value *);
struct SEE_string *string_to_SEE_string(struct SEE_interpreter *, unsigned char *);

#endif
