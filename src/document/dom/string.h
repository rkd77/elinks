#ifndef EL__DOCUMENT_DOM_STRING_H
#define EL__DOCUMENT_DOM_STRING_H

struct dom_string {
	size_t length;
	unsigned char *string;
};

#define INIT_DOM_STRING(strvalue, strlength) \
	{ (strlength) == -1 ? sizeof(strvalue) - 1 : (strlength), (strvalue) }

static inline void
set_dom_string(struct dom_string *string, unsigned char *value, uint16_t length)
{
	string->string = value;
	string->length = length;
}

#define is_dom_string_set(str) ((str)->string && (str)->length)

#define done_dom_string(str) mem_free((str)->string);

#endif
