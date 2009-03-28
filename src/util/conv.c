/** Conversion functions
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/charsets.h" /* NBSP_CHAR */
#include "util/conv.h"
#include "util/error.h"
#include "util/string.h"
#include "util/time.h"



/** This function takes string @a s and stores the @a number (of a
 * result width @a width) in string format there, starting at position
 * [*@a slen]. If the number would take more space than @a width, it
 * is truncated and only the _last_ digits of it are inserted to the
 * string. If the number takes less space than @a width, it is padded
 * by @a fillchar from left.
 * @a base defined which base should be used (10, 16, 8, 2, ...)
 * @a upper selects either hexa uppercased chars or lowercased chars.
 *
 * A NUL char is always added at the end of the string. @a s must point
 * to a sufficiently large memory space, at least *@a slen + @a width + 1.
 *
 * Examples:
 *
 * @code
 * elinks_ulongcat(s, NULL, 12345, 4, 0, 10, 0) : s = "2345"
 * elinks_ulongcat(s, NULL, 255, 4, '*', 16, 1) : s = "**FF"
 * elinks_ulongcat(s, NULL, 123, 5, '0', 10, 0) : s = "00123"
 * @endcode
 *
 * Note that this function exists to provide a fast and efficient, however
 * still quite powerful alternative to sprintf(). It is optimized for speed and
 * is *MUCH* faster than sprintf(). If you can use it, use it ;-). But do not
 * get too enthusiastic, do not use it in cases where it would break i18n.
 *
 * @returns 0 if OK or width needed for the whole number to fit there,
 * if it had to be truncated. A negative value signs an error. */
NONSTATIC_INLINE int
elinks_ulongcat(unsigned char *s, unsigned int *slen,
		unsigned long number, unsigned int width,
		unsigned char fillchar, unsigned int base,
		unsigned int upper)
{
	static const unsigned char unum[]= "0123456789ABCDEF";
	static const unsigned char lnum[]= "0123456789abcdef";
	const unsigned char *to_num = (upper ? unum : lnum);
	unsigned int start = slen ? *slen : 0;
	unsigned int nlen = 1; /* '0' is one char, we can't have less. */
	unsigned int pos = start; /* starting position of the number */
	unsigned long q = number;
	int ret = 0;

	if (width < 1 || !s || base < 2 || base > 16) return -1;

	/* Count the length of the number in chars. */
	while (q > (base - 1)) {
		nlen++;
		q /= base;
	}

	/* If max. width attained, truncate. */
	if (nlen > width) {
		ret = nlen;
		nlen = width;
	}

	if (slen) *slen += nlen;

	/* Fill left space with fillchar. */
	if (fillchar) {
		/* ie. width = 4 nlen = 2 -> pad = 2 */
		unsigned int pad = width - nlen;

		if (pad > 0) {
			/* Relocate the start of number. */
			if (slen) *slen += pad;
			pos += pad;

			/* Pad. */
			while (pad > 0) s[--pad + start] = fillchar;
		}
	}

	s[pos + nlen] = '\0';

	/* Now write number starting from end. */
	while (nlen > 0) {
		s[--nlen + pos] = to_num[(number % base)];
		number /= base;
	}

	return ret;
}

/** Similar to elinks_ulongcat() but for @c long number. */
NONSTATIC_INLINE int
elinks_longcat(unsigned char *s, unsigned int *slen,
	       long number, unsigned int width,
	       unsigned char fillchar, unsigned int base,
	       unsigned int upper)
{
	unsigned char *p = s;

	if (number < 0 && width > 0) {
		if (slen) p[(*slen)++] = '-';
		else *(p++) = '-';
		number = -number;
		width--;
	}

	return elinks_ulongcat(p, slen, number, width, fillchar, base, upper);
}


/** @relates string */
struct string *
add_long_to_string(struct string *string, long number)
{
	unsigned char buffer[32];
	int length = 0;
	int width;

	assert(string);
	if_assert_failed { return NULL; }

	width = longcat(buffer, &length, number, sizeof(buffer) - 1, 0);
	if (width < 0 || !length) return NULL;

	return add_bytes_to_string(string, buffer, length);
}

/** @relates string */
struct string *
add_knum_to_string(struct string *string, long num)
{
	int ret;
	unsigned char t[32];
	int tlen = 0;

	if (num && (num / (1024 * 1024)) * (1024 * 1024) == num) {
		ret = longcat(&t, &tlen, num / (1024 * 1024), sizeof(t) - 2, 0);
		t[tlen++] = 'M';
		t[tlen] = '\0';
	} else if (num && (num / 1024) * 1024 == num) {
		ret = longcat(&t, &tlen, num / 1024, sizeof(t) - 2, 0);
		t[tlen++] = 'k';
		t[tlen] = '\0';
	} else {
		ret = longcat(&t, &tlen, num, sizeof(t) - 1, 0);
	}

	if (ret < 0 || !tlen) return NULL;

	add_bytes_to_string(string, t, tlen);

	return string;
}

/** @relates string */
struct string *
add_xnum_to_string(struct string *string, off_t xnum)
{
	unsigned char suff[3] = "\0i";
	off_t d = -1;

	/* XXX: I don't completely like the computation of d here. --pasky */
	/* Mebi (Mi), 2^20 */
	if (xnum >= 1024 * 1024) {
		suff[0] = 'M';
		d = (xnum * (int) 10 / (int) ((int) (1024 * 1024))) % 10;
		xnum /= 1024*1024;
	/* Kibi (Ki), 2^10 */
	} else if (xnum >= 1024) {
		suff[0] = 'K';
		d = (xnum * (int) 10 / (int) 1024) % 10;
		xnum /= 1024;
	}

	assert(xnum == (long) xnum);
	add_long_to_string(string, xnum);

	if (xnum < 10 && d != -1) {
		add_char_to_string(string, '.');
		add_long_to_string(string, d);
	}
	add_char_to_string(string, ' ');

	if (suff[0]) add_to_string(string, suff);
	add_char_to_string(string, 'B');
	return string;
}

/** @relates string */
struct string *
add_duration_to_string(struct string *string, long seconds)
{
	unsigned char q[64];
	int qlen = 0;

	if (seconds < 0) seconds = 0;

	/* Days */
	if (seconds >= (24 * 3600)) {
		ulongcat(q, &qlen, (seconds / (24 * 3600)), 5, 0);
		q[qlen++] = 'd';
		q[qlen++] = ' ';
	}

	/* Hours and minutes */
	if (seconds >= 3600) {
		seconds %= (24 * 3600);
		ulongcat(q, &qlen, (seconds / 3600), 4, 0);
		q[qlen++] = ':';
		ulongcat(q, &qlen, ((seconds / 60) % 60), 2, '0');
	} else {
		/* Only minutes */
		ulongcat(q, &qlen, (seconds / 60), 2, 0);
	}

	/* Seconds */
	q[qlen++] = ':';
	ulongcat(q, &qlen, (seconds % 60), 2, '0');

	add_to_string(string, q);
	return string;
}

/** @relates string */
struct string *
add_timeval_to_string(struct string *string, timeval_T *timeval)
{
	return add_duration_to_string(string, timeval_to_seconds(timeval));
}

#ifdef HAVE_STRFTIME
struct string *
add_date_to_string(struct string *string, const unsigned char *fmt,
		   const time_t *date)
{
	unsigned char buffer[MAX_STR_LEN];
	time_t when_time = date ? *date : time(NULL);
	struct tm *when_local = localtime(&when_time);

	if (strftime(buffer, sizeof(buffer), fmt, when_local) <= 0)
		return NULL;

	return add_to_string(string, buffer);
}
#endif

/* Encoders and string changers */

struct string *
add_string_replace(struct string *string, unsigned char *src, int len,
		   unsigned char replaceable, unsigned char replacement)
{
	int oldlength = string->length;

	if (!add_bytes_to_string(string, src, len))
		return NULL;

	for (src = string->source + oldlength; len; len--, src++)
		if (*src == replaceable)
			*src = replacement;

	return string;
}

struct string *
add_html_to_string(struct string *string, const unsigned char *src, int len)
{
	for (; len; len--, src++) {
		if (*src < 0x20
		    || *src == '<' || *src == '>' || *src == '&'
		    || *src == '\"' || *src == '\'') {
			int rollback_length = string->length;

			if (!add_bytes_to_string(string, "&#", 2)
			    || !add_long_to_string(string, (long) *src)
			    || !add_char_to_string(string, ';')) {
				string->length = rollback_length;
				string->source[rollback_length] = '\0';
				return NULL;
			}
		} else {
			if (!add_char_to_string(string, *src))
				return NULL;
		}
	}

	return string;
}

struct string *
add_cp_html_to_string(struct string *string, int src_codepage,
		      const unsigned char *src, int len)
{
	const unsigned char *const end = src + len;
	unicode_val_T unicode;

	for (;;) {
		unicode = cp_to_unicode(src_codepage,
					(unsigned char **) &src, end);
		if (unicode == UCS_NO_CHAR)
			break;

		if (unicode < 0x20 || unicode >= 0x7F
		    || unicode == '<' || unicode == '>' || unicode == '&'
		    || unicode == '\"' || unicode == '\'') {
			int rollback_length = string->length;

			if (!add_bytes_to_string(string, "&#", 2)
			    || !add_long_to_string(string, unicode)
			    || !add_char_to_string(string, ';')) {
				string->length = rollback_length;
				string->source[rollback_length] = '\0';
				return NULL;
			}
		} else {
			if (!add_char_to_string(string, unicode))
				return NULL;
		}
	}

	return string;
}

/* TODO Optimize later --pasky */
struct string *
add_quoted_to_string(struct string *string, const unsigned char *src, int len)
{
	for (; len; len--, src++) {
		if (isquote(*src) || *src == '\\')
			add_char_to_string(string, '\\');
		add_char_to_string(string, *src);
	}

	return string;
}

struct string *
add_shell_quoted_to_string(struct string *string, unsigned char *src, int len)
{
	add_char_to_string(string, '\'');
	for (; len; len--, ++src)
		if (*src == '\'')
			add_to_string(string, "'\\''");
		else
			add_char_to_string(string, *src);
	add_char_to_string(string, '\'');

	return string;
}

struct string *
add_shell_safe_to_string(struct string *string, unsigned char *cmd, int cmdlen)
{
	int prev_safe = 0;

	for (; cmdlen; cmdlen--, cmd++) {
		if ((*cmd == '-' && prev_safe) ||
		    (prev_safe = is_safe_in_shell(*cmd))) {
			add_char_to_string(string, *cmd);
		} else {
			/* XXX: Not all programs we might exec are capable of
			 * decoding these.  For some, we should just report
			 * an error rather than exec with an encoded string. */
			add_char_to_string(string, '%');
			add_char_to_string(string, hx((*cmd & 0xf0) >> 4));
			add_char_to_string(string, hx(*cmd & 0x0f));
		}
	}

	return string;
}


long
strtolx(unsigned char *str, unsigned char **end)
{
	long num;
	unsigned char postfix;

	errno = 0;
	num = strtol(str, (char **) end, 10);
	if (errno) return 0;
	if (!*end) return num;

	postfix = c_toupper(**end);
	if (postfix == 'K') {
		(*end)++;
		if (num < -INT_MAX / 1024) return -INT_MAX;
		if (num > INT_MAX / 1024) return INT_MAX;
		return num * 1024;
	}

	if (postfix == 'M') {
		(*end)++;
		if (num < -INT_MAX / (1024 * 1024)) return -INT_MAX;
		if (num > INT_MAX / (1024 * 1024)) return INT_MAX;
		return num * (1024 * 1024);
	}

	return num;
}

int
month2num(const unsigned char *str)
{
	unsigned char month[3] = { str[0]|32, str[1]|32, str[2]|32 };

	switch (month[0]) {
	case 'j': /* jan, jun, jul */
		if (month[1] == 'a') {
			if (month[2] == 'n') return 0; /* jan */
			return -1;
		}
		if (month[1] == 'u') {
			if (month[2] == 'n') return 5; /* jun */
			if (month[2] == 'l') return 6; /* jul */
		}
		return -1;
	case 'm': /* mar, may */
		if (month[1] == 'a') {
			if (month[2] == 'r') return 2; /* mar */
			if (month[2] == 'y') return 4; /* may */
		}
		return -1;
	case 'a': /* apr, aug */
		if (month[1] == 'p') {
			if (month[2] == 'r') return 3; /* apr */
			return -1;
		}
		if (month[1] == 'u' && month[2] == 'g') return 7; /* aug */
		return -1;
	case 's':
		if (month[1] == 'e' && month[2] == 'p') return 8; /* sep */
		return -1;
	case 'o':
		if (month[1] == 'c' && month[2] == 't') return 9; /* oct */
		return -1;
	case 'n':
		if (month[1] == 'o' && month[2] == 'v') return 10; /* nov */
		return -1;
	case 'd':
		if (month[1] == 'e' && month[2] == 'c') return 11; /* dec */
		return -1;
	case 'f':
		if (month[1] == 'e' && month[2] == 'b') return 1; /* feb */
		return -1;
	default:
		return -1;
	}
}

/** This function drops control chars, nbsp char and limit the number
 * of consecutive space chars to one. It modifies its argument. */
void
clr_spaces(unsigned char *str)
{
	unsigned char *s;
	unsigned char *dest = str;

	assert(str);

	for (s = str; *s; s++)
		if (*s < ' ' || *s == NBSP_CHAR) *s = ' ';

	for (s = str; *s; s++) {
		if (*s == ' ' && (dest == str || s[1] == ' ' || !s[1]))
			continue;

		*dest++ = *s;
	}

	*dest = '\0';
}

/** Replace invalid chars in @a title with ' ' and trim all starting/ending
 * spaces.
 *
 * update_bookmark() assumes this function does not switch translation
 * tables.  */
void
sanitize_title(unsigned char *title)
{
	int len = strlen(title);

	if (!len) return;

	while (len--) {
		if (title[len] < ' ' || title[len] == NBSP_CHAR)
			title[len] = ' ';
	}
	trim_chars(title, ' ', NULL);
}

/** Returns 0 if @a url contains invalid chars, 1 if ok.
 * It trims starting/ending spaces. */
int
sanitize_url(unsigned char *url)
{
	int len = strlen(url);

	if (!len) return 1;

	while (len--) {
		if (url[len] < ' ')
			return 0;
	}
	trim_chars(url, ' ', NULL);
	return 1;
}


int c_tolower(int c) {
	switch (c)
	{
		case 'A': return 'a';
		case 'B': return 'b';
		case 'C': return 'c';
		case 'D': return 'd';
		case 'E': return 'e';
		case 'F': return 'f';
		case 'G': return 'g';
		case 'H': return 'h';
		case 'I': return 'i';
		case 'J': return 'j';
		case 'K': return 'k';
		case 'L': return 'l';
		case 'M': return 'm';
		case 'N': return 'n';
		case 'O': return 'o';
		case 'P': return 'p';
		case 'Q': return 'q';
		case 'R': return 'r';
		case 'S': return 's';
		case 'T': return 't';
		case 'U': return 'u';
		case 'V': return 'v';
		case 'W': return 'w';
		case 'X': return 'x';
		case 'Y': return 'y';
		case 'Z': return 'z';
		default: return c;
	}
}

int c_toupper(int c) {
	switch (c) {
		case 'a': return 'A';
		case 'b': return 'B';
		case 'c': return 'C';
		case 'd': return 'D';
		case 'e': return 'E';
		case 'f': return 'F';
		case 'g': return 'G';
		case 'h': return 'H';
		case 'i': return 'I';
		case 'j': return 'J';
		case 'k': return 'K';
		case 'l': return 'L';
		case 'm': return 'M';
		case 'n': return 'N';
		case 'o': return 'O';
		case 'p': return 'P';
		case 'q': return 'Q';
		case 'r': return 'R';
		case 's': return 'S';
		case 't': return 'T';
		case 'u': return 'U';
		case 'v': return 'V';
		case 'w': return 'W';
		case 'x': return 'X';
		case 'y': return 'Y';
		case 'z': return 'Z';
		default: return c;
	}
}

int c_isupper (int c)
{
	switch (c)
	{
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
		case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
		case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
		case 'Y': case 'Z':
			return 1;
		default:
			return 0;
	}
}

int c_islower (int c)
{
	switch (c)
	{
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
		case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
		case 's': case 't': case 'u': case 'v': case 'w': case 'x':
		case 'y': case 'z':
			return 1;
		default:
			return 0;
	}
}

