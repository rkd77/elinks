#ifndef EL_DOM_STRING_H
#define EL_DOM_STRING_H

struct dom_string {
	size_t length;
	unsigned char *string;
};

#define INIT_DOM_STRING(strvalue, strlength) \
	{ (strlength) == -1 ? sizeof(strvalue) - 1 : (strlength), (strvalue) }

static inline void
set_dom_string(struct dom_string *string, unsigned char *value, size_t length)
{
	string->string = value;
	string->length = length == -1 ? strlen(value) : length;
}

static inline int
dom_string_casecmp(struct dom_string *string1, struct dom_string *string2)
{
	size_t length = int_min(string1->length, string2->length);
	size_t string_diff = strncasecmp(string1->string, string2->string, length);

	/* If the lengths or strings don't match strncasecmp() does the
	 * job else return which ever is bigger. */
	return string_diff ? string_diff : string1->length - string2->length;
}

static inline int
dom_string_ncasecmp(struct dom_string *string1, struct dom_string *string2, size_t length)
{
	return strncasecmp(string1->string, string2->string, length);
}

#define is_dom_string_set(str) ((str)->string && (str)->length)

#define done_dom_string(str) mem_free((str)->string);

#endif
