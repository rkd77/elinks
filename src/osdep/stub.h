#ifndef EL__OSDEP_STUB_H
#define EL__OSDEP_STUB_H

#include <ctype.h>
#include <string.h>

#if 0
#ifdef CONFIG_DEBUG
#define CONFIG_OWN_LIBC
#endif
#endif

#ifdef CONFIG_OWN_LIBC

#undef HAVE_BCOPY /* prevent using bcopy() stub for memmove() */
#undef HAVE_ISDIGIT
#undef HAVE_MEMMOVE
#undef HAVE_MEMPCPY
#undef HAVE_RAISE
#undef HAVE_STPCPY
#undef HAVE_STRCASECMP
#undef HAVE_STRCASESTR
#undef HAVE_STRDUP
#undef HAVE_STRERROR
#undef HAVE_STRNCASECMP
#undef HAVE_STRSTR
#undef HAVE_INET_NTOP

#endif /* CONFIG_OWN_LIBC */


/* These stubs are exception to our "Use (unsigned char *)!" rule. This is
 * because the stubbed functions are defined using (char *), and we could get
 * in trouble with this. Or when you use (foo ? strstr() : strcasestr()) and
 * one of these is system and another stub, we're in trouble and get "Pointer
 * type mismatch in conditional expression", game over. */


/** strchr() */

#ifndef HAVE_STRCHR
#ifdef HAVE_INDEX /* for old BSD systems. */

#undef strchr
#define strchr(a, b) index(a, b)
#undef strrchr
#define strrchr(a, b) rindex(a, b)

#else /* ! HAVE_INDEX */
# error You have neither strchr() nor index() function. Please go upgrade your system.
#endif /* HAVE_INDEX */
#endif /* HAVE_STRCHR */

#ifndef HAVE_ISDIGIT
#undef isdigit
#define isdigit(a) elinks_isdigit(a)
int elinks_isdigit(int);
#endif

/** strerror() */
#ifndef HAVE_STRERROR
#undef strerror
#define strerror(e) elinks_strerror(e)
const char *elinks_strerror(int);
#endif

/** strstr() */
#ifndef HAVE_STRSTR
#undef strstr
#define strstr(a, b) elinks_strstr(a, b)
char *elinks_strstr(const char *, const char *);
#endif

/** memmove() */
#ifndef HAVE_MEMMOVE
#ifdef HAVE_BCOPY
# define memmove(dst, src, n) bcopy(src, dst, n)
#else
#undef memmove
#define memmove(dst, src, n) elinks_memmove(dst, src, n)
void *elinks_memmove(void *, const void *, size_t);
#endif
#endif

/** strcasecmp() */
#ifndef HAVE_STRCASECMP
#undef strcasecmp
#define strcasecmp(a, b) elinks_strcasecmp(a, b)
int elinks_strcasecmp(const char *, const char *);
#endif

/** strncasecmp() */
#ifndef HAVE_STRNCASECMP
#undef strncasecmp
#define strncasecmp(a, b, l) elinks_strncasecmp(a, b, l)
int elinks_strncasecmp(const char *, const char *, size_t);
#endif

/** strcasestr() */
#ifndef HAVE_STRCASESTR
#undef strcasestr
#define strcasestr(a, b) elinks_strcasestr(a, b)
char *elinks_strcasestr(const char *, const char *);
#endif

/** strdup() */
#ifndef HAVE_STRDUP
#undef strdup
#define strdup(s) elinks_strdup(s)
char *elinks_strdup(const char *);
#endif

/* stpcpy() */
#ifndef HAVE_STPCPY
#undef stpcpy
#define stpcpy(d, s) elinks_stpcpy(d, s)
char *elinks_stpcpy(char *, const char *);
#endif

/* mempcpy() */
#ifndef HAVE_MEMPCPY
#undef mempcpy
#define mempcpy(dest, src, n) elinks_mempcpy(dest, src, n)
void *elinks_mempcpy(void *, const void *, size_t);
#endif

/* memrchr() */
#ifndef HAVE_MEMRCHR
#undef memrchr
#define memrchr(src, c, n) elinks_memrchr(src, c, n)
void *elinks_memrchr(const void *s, int c, size_t n);
#endif

/* raise() */
#ifndef HAVE_RAISE
#undef raise
#define raise(signal) elinks_raise(signal)
int elinks_raise(int signal);
#endif

/* inet_ntop() */
#ifndef HAVE_INET_NTOP
#undef inet_ntop
#define inet_ntop(af, src, dst, size) elinks_inet_ntop(af, src, dst, size)
const char *elinks_inet_ntop(int af, const void *src, char *dst, size_t size);
#endif

/* Silence various sparse warnings. */

#ifndef __builtin_stpcpy
extern char *__builtin_stpcpy(char *dest, const char *src);
#endif

#ifndef __builtin_mempcpy
extern void *__builtin_mempcpy(void *dest, const void *src, size_t n);
#endif

#if 0
#ifndef __builtin_va_copy
#define __builtin_va_copy(dest, src) do { dest = src; } while (0)
#endif
#endif

#endif
