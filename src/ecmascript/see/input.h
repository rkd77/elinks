#ifndef EL__ECMASCRIPT_SEE_INPUT_H
#define EL__ECMASCRIPT_SEE_INPUT_H

#include "intl/charsets.h"

struct SEE_interpreter;
struct SEE_string;
struct SEE_value;

struct SEE_input *see_input_elinks(struct SEE_interpreter *, unsigned char *);
unsigned char *see_string_to_unsigned_char(struct SEE_string *);
unsigned char *see_value_to_unsigned_char(struct SEE_interpreter *, struct SEE_value *);
struct SEE_string *string_to_SEE_string(struct SEE_interpreter *, unsigned char *);
void append_unicode_to_SEE_string(struct SEE_interpreter *, struct SEE_string *,
				  unicode_val_T);
unicode_val_T see_string_to_unicode(struct SEE_interpreter *, struct SEE_string *);

#endif
