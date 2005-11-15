#ifndef EL__UTIL_SNPRINTF_H
#define EL__UTIL_SNPRINTF_H


/* Do not use a 'format' token here as it gets defined at some dirty stinking
 * places of ELinks, causing bug 244 (read as "compilation failures"). 'fmt'
 * should do fine. --pasky */


#include <stdarg.h>

/* XXX: This is not quite the best place for it, perhaps. But do we have
 * a better one now? --pasky */
#ifndef VA_COPY
#ifdef HAVE_VA_COPY
#define VA_COPY(dest, src) __va_copy(dest, src)
#else
#define VA_COPY(dest, src) (dest) = (src)
#endif
#endif

#ifdef CONFIG_OWN_LIBC
#undef HAVE_VSNPRINTF
#undef HAVE_C99_VSNPRINTF
#undef HAVE_SNPRINTF
#undef HAVE_C99_SNPRINTF
#undef HAVE_VASPRINTF
#undef HAVE_ASPRINTF
#else
#include <stdio.h> /* The system's snprintf(). */
#endif


#if !defined(HAVE_VSNPRINTF) || !defined(HAVE_C99_VSNPRINTF)
#undef vsnprintf
#define vsnprintf elinks_vsnprintf
int elinks_vsnprintf(char *str, size_t count, const char *fmt, va_list args);
#endif

#if !defined(HAVE_SNPRINTF) || !defined(HAVE_C99_VSNPRINTF)
#undef snprintf
#define snprintf elinks_snprintf
int elinks_snprintf(char *str, size_t count, const char *fmt, ...);
#endif


#ifndef HAVE_VASPRINTF
#undef vasprintf
#define vasprintf elinks_vasprintf
int elinks_vasprintf(char **ptr, const char *fmt, va_list ap);
#endif

#ifndef HAVE_ASPRINTF
#undef asprintf
#define asprintf elinks_asprintf
int elinks_asprintf(char **ptr, const char *fmt, ...);
#endif


/* These are wrappers for (v)asprintf() which return the strings allocated by
 * ELinks' own memory allocation routines, thus it is usable in the context of
 * standard ELinks memory managment. Just use these if you mem_free() the
 * string later and use the original ones if you free() the string later. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* We want vasprintf() */
#endif

#include <stdlib.h>
#include "util/string.h"

int vasprintf(char **ptr, const char *fmt, va_list ap);

static inline unsigned char *
vasprintfa(const char *fmt, va_list ap) {
	char *str1;
	unsigned char *str2;
	int size;

	if (vasprintf(&str1, fmt, ap) < 0)
		return NULL;

	size = strlen(str1) + 1;
	str2 = mem_alloc(size);
	if (str2) memcpy(str2, str1, size);
	free(str1);

	return str2;
}

unsigned char *asprintfa(const char *fmt, ...);


#endif
