#ifndef EL__UTIL_CONV_H
#define EL__UTIL_CONV_H

#include "util/string.h"
#include "util/time.h" /* timeval_T types */

static inline int
is_safe_in_shell(unsigned char c)
{
	/* Note: '-' is often used to indicate a command-line option and thus
	 * is not always safe. */
	return isasciialnum(c)
		|| c == '@' || c == '+' || c == '.'
		|| c == '/' || c == ':' || c == '_';
}


long strtolx(unsigned char *, unsigned char **);

/* Convert a decimal number to hexadecimal (lowercase) (0 <= a <= 15). */
static inline unsigned char
hx(register int a)
{
	return a >= 10 ? a + 'a' - 10 : a + '0';
}

/* Convert a decimal number to hexadecimal (uppercase) (0 <= a <= 15). */
static inline unsigned char
Hx(register int a)
{
	return a >= 10 ? a + 'A' - 10 : a + '0';
}

/* Convert an hexadecimal char ([0-9][a-z][A-Z]) to
 * its decimal value (0 <= result <= 15)
 * returns -1 if parameter is not an hexadecimal char. */
static inline int
unhx(register unsigned char a)
{
	if (isdigit(a)) return a - '0';
	if (a >= 'a' && a <= 'f') return a - 'a' + 10;
	if (a >= 'A' && a <= 'F') return a - 'A' + 10;
	return -1;
}

/* These use granular allocation stuff. */
struct string *add_long_to_string(struct string *string, long number);
struct string *add_knum_to_string(struct string *string, long number);
struct string *add_xnum_to_string(struct string *string, off_t number);
struct string *add_duration_to_string(struct string *string, long seconds);
struct string *add_timeval_to_string(struct string *string, timeval_T *timeval);

#ifdef HAVE_STRFTIME
/* Uses strftime() to add @fmt time format to @string. If @time is NULL
 * time(NULL) will be used. */
struct string *add_date_to_string(struct string *string, unsigned char *format, time_t *time);
#endif


/* Encoders: */
/* They encode and add to the string. This way we don't need to first allocate
 * and encode a temporary string, add it and then free it. Can be used as
 * backends for encoder. */

/* A simple generic encoder. Should maybe take @replaceable as a string so we
 * could also use it for adding shell safe strings. */
struct string *
add_string_replace(struct string *string, unsigned char *src, int len,
		   unsigned char replaceable, unsigned char replacement);

#define add_optname_to_string(str, src, len) \
	add_string_replace(str, src, len, '.', '*')

/* Maybe a bad name but it is actually the real name, but you may also think of
 * it as adding the decoded option name. */
#define add_real_optname_to_string(str, src, len) \
	add_string_replace(str, src, len, '*', '.')

/* Convert reserved chars to html &#xx */
struct string *add_html_to_string(struct string *string, unsigned char *html, int htmllen);

/* Escapes \ and " with a \ */
struct string *add_quoted_to_string(struct string *string, unsigned char *q, int qlen);

/* Adds ', |len| bytes of |src| with all single-quotes converted to '\'',
 * and ' to |string|. */
struct string *add_shell_quoted_to_string(struct string *string,
					  unsigned char *src, int len);

/* Escapes non shell safe chars with '_'. */
struct string *add_shell_safe_to_string(struct string *string, unsigned char *cmd, int cmdlen);


/* These are fast functions to convert integers to string, or to hexadecimal string. */

int elinks_ulongcat(unsigned char *s, unsigned int *slen, unsigned long number,
		    unsigned int width, unsigned char fillchar, unsigned int base,
		    unsigned int upper);

int elinks_longcat(unsigned char *s, unsigned int *slen, long number,
		   unsigned int width, unsigned char fillchar, unsigned int base,
		   unsigned int upper);

/* Type casting is enforced, to shorten calls. --Zas */
/* unsigned long to decimal string */
#define ulongcat(s, slen, number, width, fillchar) \
	elinks_ulongcat((unsigned char *) (s), \
			(unsigned int *) (slen), \
			(unsigned long) (number), \
			(unsigned int) (width), \
			(unsigned char) (fillchar), \
			(unsigned int) 10, \
			(unsigned int) 0)

/* signed long to decimal string */
#define longcat(s, slen, number, width, fillchar) \
	 elinks_longcat((unsigned char *) (s), \
			(unsigned int *) (slen), \
			(long) (number), \
			(unsigned int) (width), \
			(unsigned char) (fillchar), \
			(unsigned int) 10, \
			(unsigned int) 0)

/* unsigned long to hexadecimal string */
#define ulonghexcat(s, slen, number, width, fillchar, upper) \
	elinks_ulongcat((unsigned char *) (s), \
			(unsigned int *) (slen), \
			(unsigned long) (number), \
			(unsigned int) (width), \
			(unsigned char) (fillchar), \
			(unsigned int) 16, \
			(unsigned int) (upper))


/* XXX: Compatibility only. Remove these at some time. --Zas */
#define snprint(str, len, num) ulongcat(str, NULL, num, len, 0);
#define snzprint(str, len, num) longcat(str, NULL, num, len, 0);

/* Return 0 if starting with jan, 11 for dec, -1 for failure.
 * @month must be a lowercased string. */
int month2num(const unsigned char *month);

#include <string.h>

/* Trim starting and ending chars equal to @c in string @s.
 * If @len != NULL, it stores new string length in pointed integer.
 * It returns @s for convenience. */
static inline unsigned char *
trim_chars(unsigned char *s, unsigned char c, int *len)
{
	int l = strlen(s);
	unsigned char *p = s;

	while (*p == c) p++, l--;
	while (l && p[l - 1] == c) p[--l] = '\0';

	memmove(s, p, l + 1);
	if (len) *len = l;

	return s;
}

/* Convert uppercase letters in @string with the given @length to lowercase. */
static inline void
convert_to_lowercase(unsigned char *string, int length)
{
	for (length--; length >= 0; length--)
		if (isupper(string[length]))
			string[length] = tolower(string[length]);
}

/* This function drops control chars, nbsp char and limit the number of consecutive
 * space chars to one. It modifies its argument. */
void clr_spaces(unsigned char *str);

/* Replace invalid chars in @title with ' ' and trim all starting/ending
 * spaces. */
void sanitize_title(unsigned char *title);

/* Returns 0 if @url contains invalid chars, 1 if ok.
 * It trims starting/ending spaces. */
int sanitize_url(unsigned char *url);

#endif
