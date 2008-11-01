/* Parser of HTTP headers */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "protocol/header.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/*
* RFC 2616                        HTTP/1.1                       June 1999
*
*
*        OCTET          = <any 8-bit sequence of data>
*        CHAR           = <any US-ASCII character (octets 0 - 127)>
*        UPALPHA        = <any US-ASCII uppercase letter "A".."Z">
*        LOALPHA        = <any US-ASCII lowercase letter "a".."z">
*        ALPHA          = UPALPHA | LOALPHA
*        DIGIT          = <any US-ASCII digit "0".."9">
*        CTL            = <any US-ASCII control character
*                         (octets 0 - 31) and DEL (127)>
*        CR             = <US-ASCII CR, carriage return (13)>
*        LF             = <US-ASCII LF, linefeed (10)>
*        SP             = <US-ASCII SP, space (32)>
*        HT             = <US-ASCII HT, horizontal-tab (9)>
*        <">            = <US-ASCII double-quote mark (34)>
*
*    HTTP/1.1 defines the sequence CR LF as the end-of-line marker for all
*    protocol elements except the entity-body (see appendix 19.3 for
*    tolerant applications). The end-of-line marker within an entity-body
*    is defined by its associated media type, as described in section 3.7.
*
*        CRLF           = CR LF
*
*    HTTP/1.1 header field values can be folded onto multiple lines if the
*    continuation line begins with a space or horizontal tab. All linear
*    white space, including folding, has the same semantics as SP. A
*    recipient MAY replace any linear white space with a single SP before
*    interpreting the field value or forwarding the message downstream.
*
*        LWS            = [CRLF] 1*( SP | HT )
*
*    The TEXT rule is only used for descriptive field contents and values
*    that are not intended to be interpreted by the message parser. Words
*    of *TEXT MAY contain characters from character sets other than ISO-
*    8859-1 [22] only when encoded according to the rules of RFC 2047
*    [14].
*
*        TEXT           = <any OCTET except CTLs,
*                         but including LWS>
*
*    A CRLF is allowed in the definition of TEXT only as part of a header
*    field continuation. It is expected that the folding LWS will be
*    replaced with a single SP before interpretation of the TEXT value.
*
*    Hexadecimal numeric characters are used in several protocol elements.
*
*        HEX            = "A" | "B" | "C" | "D" | "E" | "F"
*                       | "a" | "b" | "c" | "d" | "e" | "f" | DIGIT
*
*    Many HTTP/1.1 header field values consist of words separated by LWS
*    or special characters. These special characters MUST be in a quoted
*    string to be used within a parameter value (as defined in section
*    3.6).
*
*        token          = 1*<any CHAR except CTLs or separators>
*        separators     = "(" | ")" | "<" | ">" | "@"
*                       | "," | ";" | ":" | "\" | <">
*                       | "/" | "[" | "]" | "?" | "="
*                       | "{" | "}" | SP | HT
*
*    Comments can be included in some HTTP header fields by surrounding
*    the comment text with parentheses. Comments are only allowed in
*    fields containing "comment" as part of their field value definition.
*    In all other fields, parentheses are considered part of the field
*    value.
*
*        comment        = "(" *( ctext | quoted-pair | comment ) ")"
*        ctext          = <any TEXT excluding "(" and ")">
*
*    A string of text is parsed as a single word if it is quoted using
*    double-quote marks.
*
*        quoted-string  = ( <"> *(qdtext | quoted-pair ) <"> )
*        qdtext         = <any TEXT except <">>
*
*    The backslash character ("\") MAY be used as a single-character
*    quoting mechanism only within quoted-string and comment constructs.
*
*        quoted-pair    = "\" CHAR
*/

/* FIXME: bug 549
 *
 * HTTP/1.1 header continuation lines are not honoured.
 * DEL char is accepted in TEXT part.
 * HT char is not accepted in TEXT part.
 * LF alone do not mark end of line, CRLF is the correct termination.
 * CR or LF are invalid in header line.
 *
 * Mozilla, IE, NS tolerate header value separator different from ':'
 * Examples:
 * name: value
 * name value
 * name :value
 * name=value
 */

#define LWS(c) ((c) == ' ' || (c) == ASCII_TAB)

unsigned char *
parse_header(unsigned char *head, unsigned char *item, unsigned char **ptr)
{
	unsigned char *pos = head;

	if (!pos) return NULL;

	while (*pos) {
		unsigned char *end, *itempos, *value;
		int len;

		/* Go for a newline. */
		while (*pos && *pos != ASCII_LF) pos++;
		if (!*pos) break;
		pos++; /* Start of line now. */

		/* Does item match header line ? */
		for (itempos = item; *itempos && *pos; itempos++, pos++)
			if (c_toupper(*itempos) != c_toupper(*pos))
				break;

		if (!*pos) break; /* Nothing left to parse. */
		if (*itempos) continue; /* Do not match. */

		/* Be tolerant: we accept headers with
		 * weird syntax, since most browsers does it
		 * anyway, ie:
		 * name value
		 * name :value
		 * name = value
		 * name[TAB]:[TAB]value */

		end = pos;

		/* Skip leading whitespaces if any. */
		while (LWS(*pos)) pos++;
		if (!*pos) break; /* Nothing left to parse. */

		/* Eat ':' or '=' if any. */
		if (*pos == ':' || *pos == '=') pos++;
		if (!*pos) break; /* Nothing left to parse. */

		/* Skip whitespaces after separator if any. */
		while (LWS(*pos)) pos++;
		if (!*pos) break; /* Nothing left to parse. */

		if (pos == end) continue; /* Not an exact match (substring). */

		/* Find the end of line/string.
		 * We fail on control chars and DEL char. */
		end = pos;
		while (*end != ASCII_DEL && (*end > ' ' || LWS(*end))) end++;
		if (!*end) break; /* No end of line, nothing left to parse. */

		/* Ignore line if we encountered an unexpected char. */
		if (*end != ASCII_CR && *end != ASCII_LF) continue;

		/* Strip trailing whitespaces. */
		while (end > pos && LWS(end[-1])) end--;

		len = end - pos;
		assert(len >= 0);
		if_assert_failed break;

		if (!len) continue;	/* Empty value. */

		value = memacpy(pos, len);
		if (!value) break; /* Allocation failure, stop here. */

		if (ptr) *ptr = pos;
		return value;
	}

	return NULL;
}

/* Extract the value of name part of the value of attribute content.
 * Ie. @name = "charset" and @str = "text/html; charset=iso-8859-1"
 * will store in *@ret an allocated string containing "iso-8859-1".
 * It supposes that separator is ';' and ignore first element in the
 * list. (ie. '1' is ignored in "1; URL=xxx")
 * The return value is one of:
 *
 * - HEADER_PARAM_FOUND: the parameter was found, copied, and stored in *@ret.
 * - HEADER_PARAM_NOT_FOUND: the parameter is not there.  *@ret is now NULL.
 * - HEADER_PARAM_OUT_OF_MEMORY: error. *@ret is now NULL.
 *
 * If @ret is NULL, then this function doesn't actually access *@ret,
 * and cannot fail with HEADER_PARAM_OUT_OF_MEMORY.  Some callers may
 * rely on this. */
enum parse_header_param
parse_header_param(unsigned char *str, unsigned char *name, unsigned char **ret)
{
	unsigned char *p = str;
	int namelen, plen = 0;

	if (ret) *ret = NULL;	/* default in case of early return */

	assert(str && name && *name);
	if_assert_failed return HEADER_PARAM_NOT_FOUND;

	/* Returns now if string @str is empty. */
	if (!*p) return HEADER_PARAM_NOT_FOUND;

	namelen = strlen(name);
	do {
		p = strchr(p, ';');
		if (!p) return HEADER_PARAM_NOT_FOUND;

		while (*p && (*p == ';' || *p <= ' ')) p++;
		if (strlen(p) < namelen) return HEADER_PARAM_NOT_FOUND;
	} while (c_strncasecmp(p, name, namelen));

	p += namelen;

	while (*p && (*p <= ' ' || *p == '=')) p++;
	if (!*p) {
		if (ret) {
			*ret = stracpy("");
			if (!*ret)
				return HEADER_PARAM_OUT_OF_MEMORY;
		}
		return HEADER_PARAM_FOUND;
	}

	while ((p[plen] > ' ' || LWS(p[plen])) && p[plen] != ';') plen++;

	/* Trim ending spaces */
	while (plen > 0 && LWS(p[plen - 1])) plen--;

	/* XXX: Drop enclosing single quotes if there's some.
	 *
	 * Some websites like newsnow.co.uk are using single quotes around url
	 * in URL field in meta tag content attribute like this:
	 * <meta http-equiv="Refresh" content="0; URL='http://www.site.com/path/xxx.htm'">
	 *
	 * This is an attempt to handle that, but it may break something else.
	 * We drop all pair of enclosing quotes found (eg. '''url''' => url).
	 * Please report any issue related to this. --Zas */
	while (plen > 1 && *p == '\'' && p[plen - 1] == '\'') {
		p++;
		plen -= 2;
	}

	if (ret) {
		*ret = memacpy(p, plen);
		if (!*ret)
			return HEADER_PARAM_OUT_OF_MEMORY;
	}
	return HEADER_PARAM_FOUND;
}

/* Parse string param="value", return value as new string or NULL if any
 * error. */
unsigned char *
get_header_param(unsigned char *e, unsigned char *name)
{
	unsigned char *n, *start;

again:
	while (*e && c_toupper(*e++) != c_toupper(*name));
	if (!*e) return NULL;

	n = name + 1;
	while (*n && c_toupper(*e) == c_toupper(*n)) e++, n++;
	if (*n) goto again;

	skip_space(e);
	if (*e++ != '=') return NULL;

	skip_space(e);
	start = e;

	if (!isquote(*e)) {
		skip_nonspace(e);
	} else {
		unsigned char uu = *e++;

		start++;
		while (*e != uu) {
			if (!*e) return NULL;
			e++;
		}
	}

	while (start < e && *start == ' ') start++;
	while (start < e && *(e - 1) == ' ') e--;
	if (start == e) return NULL;

	n = mem_alloc(e - start + 1);
	if (n) {
		int i = 0;

		while (start < e) {
			n[i++] = (*start < ' ') ? '.' : *start;
			start++;
		}
		n[i] = '\0';
	}

	return n;
}
