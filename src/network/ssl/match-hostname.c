/* Matching a host name to wildcards in SSL certificates */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/charsets.h"
#include "network/ssl/match-hostname.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/string.h"

/** Checks whether a host name matches a pattern that may contain
 * wildcards.
 *
 * @param[in] hostname
 *   The host name to which the user wanted to connect.
 *   Should be in UTF-8 and need not be null-terminated.
 * @param[in] hostname_length
 *   The length of @a hostname, in bytes.
 * @param[in] pattern
 *   A pattern that the host name might match.
 *   Should be in UTF-8 and need not be null-terminated.
 *   The pattern may contain wildcards, as specified in
 *   RFC 2818 section 3.1.
 * @param[in] pattern_length
 *   The length of @a pattern, in bytes.
 *
 * @return
 *   Nonzero if the host name matches.  Zero if it doesn't.
 *
 * According to RFC 2818 section 3.1, '*' matches any number of
 * characters except '.'.  For example, "*r*.example.org" matches
 * "random.example.org" or "history.example.org" but not
 * "frozen.fruit.example.org".
 *
 * This function does not allocate memory, and consumes at most
 * O(@a hostname_length * @a pattern_length) time.  */
int
match_hostname_pattern(const unsigned char *hostname,
		       size_t hostname_length,
		       const unsigned char *pattern,
		       size_t pattern_length)
{
	const unsigned char *const hostname_end = hostname + hostname_length;
	const unsigned char *const pattern_end = pattern + pattern_length;

	assert(hostname <= hostname_end);
	assert(pattern <= pattern_end);
	if_assert_failed return 0;

	while (pattern < pattern_end) {
		if (*pattern == '*') {
			const unsigned char *next_wildcard;
			size_t literal_length;

			++pattern;
			next_wildcard = memchr(pattern, '*',
					       pattern_end - pattern);
			if (next_wildcard == NULL)
				literal_length = pattern_end - pattern;
			else
				literal_length = next_wildcard - pattern;

			for (;;) {
				size_t hostname_left = hostname_end - hostname;
				unicode_val_T uni;

				if (hostname_left < literal_length)
					return 0;

				/* If next_wildcard == NULL, then the
				 * literal string is at the end of the
				 * pattern, so anchor the match to the
				 * end of the hostname.  The end of
				 * this function can then easily
				 * verify that the whole hostname was
				 * matched.
				 *
				 * But do not jump directly there;
				 * first verify that there are no '.'
				 * characters in between.  */
				if ((next_wildcard != NULL
				     || hostname_left == literal_length)
				    && !c_strlcasecmp(pattern, literal_length,
						      hostname, literal_length))
					break;

				/* The literal string doesn't match here.
				 * Skip one character of the hostname and
				 * retry.  If the skipped character is '.'
				 * or one of the equivalent characters
				 * listed in RFC 3490 section 3.1
				 * requirement 1, then return 0, because
				 * '*' must not match such characters.
				 * Do the same if invalid UTF-8 is found.
				 * Cast away const.  */
				uni = utf8_to_unicode((unsigned char **) &hostname,
						      hostname_end);
				if (uni == 0x002E
				    || uni == 0x3002
				    || uni == 0xFF0E
				    || uni == 0xFF61
				    || uni == UCS_NO_CHAR)
					return 0;
			}

			pattern += literal_length;
			hostname += literal_length;
		} else {
			if (hostname == hostname_end)
				return 0;

			if (c_toupper(*pattern) != c_toupper(*hostname))
				return 0;

			++pattern;
			++hostname;
		}
	}

	return hostname == hostname_end;
}
