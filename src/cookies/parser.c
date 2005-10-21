/* Cookies name-value pairs parser  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "cookies/parser.h"
#include "util/string.h"


/* In order to be able to compile parsetst, you should try to use minimum
 * of foreign stuff here. */

#if 0
static inline void
debug_cookie_parser(struct cookie_str *cstr, unsigned char *pos, int ws, int eq)
{
	int namelen = int_max(cstr->nam_end - cstr->str, 0);
	int valuelen = int_max(cstr->val_end - cstr->val_start, 0);

	printf("[%.*s] :: (%.*s) :: %d,%d [%s] %d\n",
	       namelen, cstr->str,
	       valuelen, cstr->val_start,
	       ws, eq, pos, cstr->nam_end - cstr->str);
}
#else
#define debug_cookie_parser(cstr, pos, ws, eq)
#endif


/* This function parses the starting name/value pair from the cookie string.
 * The syntax is simply: <name token> [ '=' <value token> ] with possible
 * spaces between tokens and '='. However spaces in the value token is also
 * allowed. See bug 174 for a description why. */
/* Defined in RFC 2965. */
/* Return cstr on success, NULL on failure. */
struct cookie_str *
parse_cookie_str(struct cookie_str *cstr, unsigned char *str)
{
	memset(cstr, 0, sizeof(*cstr));
	cstr->str = str;

	/* Parse name token */
	while (*str != ';' && *str != '=' && !isspace(*str) && *str)
		str++;

	/* Bail out if name token is empty */
	if (str == cstr->str) return NULL;

	cstr->nam_end = str;

	skip_space(str);

	switch (*str) {
	case '\0':
	case ';':
		/* No value token, so just set to empty value */
		cstr->val_start = str;
		cstr->val_end = str;
		return cstr;

	case '=':
		/* Map 'a===b' to 'a=b' */
		do str++; while (*str == '=');
		break;

	default:
		/* No spaces in the name token is allowed */
		return NULL;
	}

	skip_space(str);

	/* Parse value token */

	/* Start with empty value, so even 'a=' will work */
	cstr->val_start = str;
	cstr->val_end = str;

	for (; *str != ';' && *str; str++) {
		/* Allow spaces in the value but leave out ending spaces */
		if (!isspace(*str))
			cstr->val_end = str + 1;
	}

	return cstr;
}
