/* Libc stub functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>		/* SunOS needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>		/* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "osdep/stub.h"
#include "util/conv.h"

/* These stubs are exception to our "Use (unsigned char *)!" rule. This is
 * because the stubbed functions are defined using (char *), and we could get
 * in trouble with this. Or when you use (foo ? strstr() : strcasestr()) and
 * one of these is system and another stub, we're in trouble and get "Pointer
 * type mismatch in conditional expression", game over. */


#define toupper_equal(s1, s2) (toupper(*((char *) s1)) == toupper(*((char *) s2)))
#define toupper_delta(s1, s2) (toupper(*((char *) s1)) - toupper(*((char *) s2)))

#ifndef HAVE_STRCASECMP
NONSTATIC_INLINE int
elinks_strcasecmp(const char *s1, const char *s2)
{
	while (*s1 != '\0' && toupper_equal(s1, s2)) {
		s1++;
		s2++;
	}

	return toupper_delta(s1, s2);
}
#endif /* !HAVE_STRCASECMP */

#ifndef HAVE_STRNCASECMP
NONSTATIC_INLINE int
elinks_strncasecmp(const char *s1, const char *s2, size_t len)
{
	if (len == 0)
		return 0;

	while (len-- != 0 && toupper_equal(s1, s2)) {
		if (len == 0 || *s1 == '\0' || *s2 == '\0')
			return 0;
		s1++;
		s2++;
	}

	return toupper_delta(s1, s2);
}
#endif /* !HAVE_STRNCASECMP */

#ifndef HAVE_STRCASESTR
/* Stub for strcasestr(), GNU extension */
NONSTATIC_INLINE char *
elinks_strcasestr(const char *haystack, const char *needle)
{
	size_t haystack_length = strlen(haystack);
	size_t needle_length = strlen(needle);
	int i;

	if (haystack_length < needle_length)
		return NULL;

	for (i = haystack_length - needle_length + 1; i; i--) {
		if (!strncasecmp(haystack, needle, needle_length))
			return (char *) haystack;
		haystack++;
	}

	return NULL;
}
#endif

#ifndef HAVE_STRDUP
NONSTATIC_INLINE char *
elinks_strdup(const char *str)
{
	int str_len = strlen(str);
	char *new = malloc(str_len + 1);

	if (new) {
		if (str_len) memcpy(new, str, str_len);
		new[str_len] = '\0';
	}
	return new;
}
#endif

#ifndef HAVE_STRERROR
/* Many older systems don't have this, but have the global sys_errlist array
 * instead. */
#if 0
extern int sys_nerr;
extern const char *const sys_errlist[];
#endif
NONSTATIC_INLINE const char *
elinks_strerror(int err_no)
{
	if (err_no < 0 || err_no > sys_nerr)
		return (const char *) "Unknown Error";
	else
		return (const char *) sys_errlist[err_no];
}
#endif

#ifndef HAVE_STRSTR
/* From http://www.unixpapa.com/incnote/string.html */
NONSTATIC_INLINE char *
elinks_strstr(const char *s, const char *p)
{
	char *sp, *pp;

	for (sp = (char *) s, pp = (char *) p; *sp && *pp; )
	{
		if (*sp == *pp) {
			sp++;
			pp++;
		} else {
			sp = sp - (pp - p) + 1;
			pp = (char *) p;
		}
	}
	return (*pp ? NULL : sp - (pp - p));
}
#endif

#if !defined(HAVE_MEMMOVE) && !defined(HAVE_BCOPY)
/* The memmove() function is a bit rarer than the other memory functions -
 * some systems that have the others don't have this. It is identical to
 * memcpy() but is guaranteed to work even if the strings overlap.
 * Most systems that don't have memmove() do have
 * the BSD bcopy() though some really old systems have neither.
 * Note that bcopy() has the order of the source and destination
 * arguments reversed.
 * From http://www.unixpapa.com/incnote/string.html */
/* Modified for ELinks by Zas. */
NONSTATIC_INLINE void *
elinks_memmove(void *d, const void *s, size_t n)
{
	register char *dst = (char *) d;
	register char *src = (char *) s;

	if (!n || src == dst) return (void *) dst;

	if (src > dst)
		for (; n > 0; n--)
			*(dst++) = *(src++);
	else
		for (dst += n - 1, src += n - 1;
		     n > 0;
		     n--)
			*(dst--) = *(src--);

	return (void *) dst;
}
#endif


#ifndef HAVE_STPCPY
NONSTATIC_INLINE char *
elinks_stpcpy(char *dest, const char *src)
{
	while ((*dest++ = *src++));
	return (dest - 1);
}
#endif

#ifndef HAVE_MEMPCPY
NONSTATIC_INLINE void *
elinks_mempcpy(void *dest, const void *src, size_t n)
{
	return (void *) ((char *) memcpy(dest, src, n) + n);
}
#endif

#ifndef HAVE_ISDIGIT
NONSTATIC_INLINE int
elinks_isdigit(int i)
{
	return i >= '0' && i <= '9';
}
#endif

#ifndef HAVE_MEMRCHR
NONSTATIC_INLINE void *
elinks_memrchr(const void *s, int c, size_t n)
{
	char *pos = (char *) s;

	while (n > 0) {
		if (pos[n - 1] == (char) c)
			return (void *) &pos[n - 1];
		n--;
	}

	return NULL;
}
#endif

#ifndef HAVE_RAISE

#if !defined(HAVE_KILL) || !defined(HAVE_GETPID)
#error The raise() stub function requires kill() and getpid()
#endif

int
elinks_raise(int signal)
{
	return(kill(getpid(), signal));
}
#endif

#ifndef HAVE_INET_NTOP
/* Original code by Paul Vixie. Modified by Gisle Vanem. */

#define	IN6ADDRSZ	16
#define	INADDRSZ	 4
#define	INT16SZ		 2

/* TODO: Move and populate. --jonas */
#ifdef CONFIG_OS_WIN32
#define SET_ERRNO(e)    WSASetLastError(errno = (e))
#else
#define SET_ERRNO(e)    errno = e
#endif

/* Format an IPv4 address, more or less like inet_ntoa().
 *
 * Returns `dst' (as a const)
 * Note:
 *  - uses no statics
 *  - takes a unsigned char * not an in_addr as input */
static const char *
elinks_inet_ntop4(const unsigned char *src, unsigned char *dst, size_t size)
{
	const unsigned char *addr = inet_ntoa(*(struct in_addr*)src);

	if (strlen(addr) >= size) {
		SET_ERRNO(ENOSPC);
		return NULL;
	}

	return strcpy(dst, addr);
}

#ifdef CONFIG_IPV6
/* Convert IPv6 binary address into presentation (printable) format. */
static const char *
elinks_inet_ntop6(const unsigned char *src, char *dst, size_t size)
{
	/* Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX. */
	unsigned char tmp[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
	unsigned char *tp;
	struct {
		long base;
		long len;
	} best, cur;
	unsigned long words[IN6ADDRSZ / INT16SZ];
	int i;

	/* Preprocess:
	 *  Copy the input (bytewise) array into a wordwise array.
	 *  Find the longest run of 0x00's in src[] for :: shorthanding. */
	memset(words, 0, sizeof(words));
	for (i = 0; i < IN6ADDRSZ; i++)
		words[i/2] |= (src[i] << ((1 - (i % 2)) << 3));

	best.base = -1;
	cur.base  = -1;
	for (i = 0; i < (IN6ADDRSZ / INT16SZ); i++) {
		if (words[i] == 0) {
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;

		} else if (cur.base != -1) {
			if (best.base == -1 || cur.len > best.len)
				best = cur;
			cur.base = -1;
		}
	}

	if ((cur.base != -1) && (best.base == -1 || cur.len > best.len))
		best = cur;
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/* Format the result. */
	for (tp = tmp, i = 0; i < (IN6ADDRSZ / INT16SZ); i++) {
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base && i < (best.base + best.len)) {
			if (i == best.base)
				*tp++ = ':';
			continue;
		}

		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0) *tp++ = ':';

		/* Is this address an encapsulated IPv4? */
		if (i == 6 && best.base == 0
		    && (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {

			if (!elinks_inet_ntop4(src + 12, tp, sizeof(tmp) - (tp - tmp))) {
				SET_ERRNO(ENOSPC);
				return NULL;
			}

			tp += strlen(tp);
			break;
		}

		tp += snprintf(tp, 5, "%lx", words[i]);
	}

	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == (IN6ADDRSZ / INT16SZ))
		*tp++ = ':';
	*tp++ = '\0';

	/* Check for overflow, copy, and we're done. */
	if ((size_t) (tp - tmp) > size) {
		SET_ERRNO(ENOSPC);
		return NULL;
	}

	return strcpy(dst, tmp);
}
#endif  /* CONFIG_IPV6 */

/* Convert a network format address to presentation format.
 *
 * Returns pointer to presentation format address (`dst'),
 * Returns NULL on error (see errno). */
const char *
elinks_inet_ntop(int af, const void *src, char *dst, size_t size)

{
	switch (af) {
	case AF_INET:
		return elinks_inet_ntop4((const unsigned char *) src, dst, size);
#ifdef CONFIG_IPV6
	case AF_INET6:
		return elinks_inet_ntop6((const unsigned char *) src, dst, size);
#endif
	default:
		SET_ERRNO(EAFNOSUPPORT);
		return NULL;
	}
}
#endif /* HAVE_INET_NTOP */
