/** String handling functions
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: fseeko, ftello */
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "elinks.h"

#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/snprintf.h"


/* This file looks to be slowly being overloaded by a lot of various stuff,
 * like memory managment, stubs, tools, granular and non-granular strings,
 * struct string object... Perhaps util/memory.* and util/stubs.* (stubs.h
 * probably included in elinks.h, it's important enough) would be nice to
 * have. --pasky */


#define string_assert(f, l, x, o) \
	if ((assert_failed = !(x))) { \
		errfile = f, errline = l, \
		elinks_internal("[%s] assertion %s failed!", o, #x); \
	}

#ifdef DEBUG_MEMLEAK

unsigned char *
debug_memacpy(const unsigned char *f, int l, const unsigned char *src, int len)
{
	unsigned char *m;

	string_assert(f, l, len >= 0, "memacpy");
	if_assert_failed len = 0;

	m = debug_mem_alloc(f, l, len + 1);
	if (!m) return NULL;

	if (src && len) memcpy(m, src, len);
	m[len] = '\0';

	return m;
}

unsigned char *
debug_stracpy(const unsigned char *f, int l, const unsigned char *src)
{
	string_assert(f, l, src, "stracpy");
	if_assert_failed return NULL;

	return debug_memacpy(f, l, src, strlen(src));
}

#else /* DEBUG_MEMLEAK */

unsigned char *
memacpy(const unsigned char *src, int len)
{
	unsigned char *m;

	assertm(len >= 0, "[memacpy]");
	if_assert_failed { len = 0; }

	m = mem_alloc(len + 1);
	if (!m) return NULL;

	if (src && len) memcpy(m, src, len);
	m[len] = 0;

	return m;
}

unsigned char *
stracpy(const unsigned char *src)
{
	assertm(src, "[stracpy]");
	if_assert_failed return NULL;

	return memacpy(src, strlen(src));
}

#endif /* DEBUG_MEMLEAK */


void
add_to_strn(unsigned char **dst, const unsigned char *src)
{
	unsigned char *newdst;
	int dstlen;
	int srclen;

	assertm(*dst && src, "[add_to_strn]");
	if_assert_failed return;

	dstlen = strlen(*dst);
	srclen = strlen(src) + 1; /* Include the NUL char! */
	newdst = mem_realloc(*dst, dstlen + srclen);
	if (!newdst) return;

	memcpy(newdst + dstlen, src, srclen);
	*dst = newdst;
}

unsigned char *
insert_in_string(unsigned char **dst, int pos,
		 const unsigned char *seq, int seqlen)
{
	int dstlen = strlen(*dst);
	unsigned char *string = mem_realloc(*dst, dstlen + seqlen + 1);

	if (!string) return NULL;

	memmove(string + pos + seqlen, string + pos, dstlen - pos + 1);
	memcpy(string + pos, seq, seqlen);
	*dst = string;

	return string;
}

unsigned char *
straconcat(const unsigned char *str, ...)
{
	va_list ap;
	const unsigned char *a;
	unsigned char *s;
	unsigned int len;

	assertm(str != NULL, "[straconcat]");
	if_assert_failed { return NULL; }

	len = strlen(str);
	s = mem_alloc(len + 1);
	if (!s) return NULL;

	if (len) memcpy(s, str, len);

	va_start(ap, str);
	while ((a = va_arg(ap, const unsigned char *))) {
		unsigned int l = strlen(a);
		unsigned char *ns;

		if (!l) continue;

		ns = mem_realloc(s, len + 1 + l);
		if (!ns) {
			mem_free(s);
			va_end(ap);
			return NULL;
		}

		s = ns;
		memcpy(s + len, a, l);
		len += l;
	}
	va_end(ap);

	s[len] = '\0';
	return s;
}

int
xstrcmp(const unsigned char *s1, const unsigned char *s2)
{
	if (!s1) return -!!s2;
	if (!s2) return 1;
	return strcmp(s1, s2);
}

unsigned char *
safe_strncpy(unsigned char *dst, const unsigned char *src, size_t dst_size)
{
	assertm(dst && src && dst_size > 0, "[safe_strncpy]");
	if_assert_failed return NULL;

	strncpy(dst, src, dst_size);
	dst[dst_size - 1] = '\0';

	return dst;
}

#define strlcmp_device(c,s1,n1,s2,n2,t1,t2) { \
	size_t p; \
	int d; \
 \
	/* XXX: The return value is inconsistent. Hrmpf. Making it consistent
	 * would make the @n1 != @n2 case significantly more expensive, though.
	 * So noone should better rely on the return value actually meaning
	 * anything quantitatively. --pasky */ \
 \
 	if (!s1 || !s2) \
		return 1; \
 \
	/* n1,n2 is unsigned, so don't assume -1 < 0 ! >:) */ \
 \
	/* TODO: Don't precompute strlen()s but rather make the loop smarter.
	 * --pasky */ \
	if (n1 == -1) n1 = strlen(s1); \
	if (n2 == -1) n2 = strlen(s2); \
 \
	string_assert(errfile, errline, n1 >= 0 && n2 >= 0, c); \
 \
	d = n1 - n2; \
	if (d) return d; \
 \
	for (p = 0; p < n1 && s1[p] && s2[p]; p++) { \
		d = t1 - t2; \
 		if (d) return d; \
	} \
	return 0; \
}

int
elinks_strlcmp(const unsigned char *s1, size_t n1,
	       const unsigned char *s2, size_t n2)
{
	strlcmp_device("strlcmp", s1, n1, s2, n2, s1[p], s2[p]);
}

int
elinks_strlcasecmp(const unsigned char *s1, size_t n1,
		   const unsigned char *s2, size_t n2,
		   const int locale_indep)
{
	if (locale_indep) {
		strlcmp_device("strlcasecmp", s1, n1, s2, n2, c_toupper(s1[p]), c_toupper(s2[p]));
	}
	else {
		strlcmp_device("strlcasecmp", s1, n1, s2, n2, toupper(s1[p]), toupper(s2[p]));
	}
}

int
c_strcasecmp(const char *s1, const char *s2)
{
	for (;; s1++, s2++) {
		unsigned char c1 = c_tolower(*(const unsigned char *) s1);
		unsigned char c2 = c_tolower(*(const unsigned char *) s2);
		
		if (c1 != c2)
			return (c1 < c2) ? -1: +1;
		if (c1 == '\0')
			return 0;
	}
}

int c_strncasecmp(const char *s1, const char *s2, size_t n)
{
	for (; n > 0; n--, s1++, s2++) {
		unsigned char c1 = c_tolower(*(const unsigned char *) s1);
		unsigned char c2 = c_tolower(*(const unsigned char *) s2);
		
		if (c1 != c2)
			return (c1 < c2) ? -1: +1;
		if (c1 == '\0')
			return 0;
	}
	return 0;
}

/* c_strcasestr - adapted from src/osdep/stub.c */
char * c_strcasestr(const char *haystack, const char *needle)
{
	size_t haystack_length = strlen(haystack);
	size_t needle_length = strlen(needle);
	int i;

	if (haystack_length < needle_length)
		return NULL;

	for (i = haystack_length - needle_length + 1; i; i--) {
		if (!c_strncasecmp(haystack, needle, needle_length))
			return (char *) haystack;
		haystack++;
	}

	return NULL;
}

/* The new string utilities: */

/* TODO Currently most of the functions use add_bytes_to_string() as a backend
 *	instead we should optimize each function. */

NONSTATIC_INLINE struct string *
#ifdef DEBUG_MEMLEAK
init_string__(const unsigned char *file, int line, struct string *string)
#else
init_string(struct string *string)
#endif
{
	assertm(string != NULL, "[init_string]");
	if_assert_failed { return NULL; }

	string->length = 0;
#ifdef DEBUG_MEMLEAK
	string->source = debug_mem_alloc(file, line, STRING_GRANULARITY + 1);
#else
	string->source = mem_alloc(STRING_GRANULARITY + 1);
#endif
	if (!string->source) return NULL;

	*string->source = '\0';

	set_string_magic(string);

	return string;
}

NONSTATIC_INLINE void
done_string(struct string *string)
{
	assertm(string != NULL, "[done_string]");
	if_assert_failed { return; }

	if (string->source) {
		/* We only check the magic if we have to free anything so
		 * that done_string() can be called multiple times without
		 * blowing up something */
		check_string_magic(string);
		mem_free(string->source);
	}

	/* Blast everything including the magic */
	memset(string, 0, sizeof(*string));
}

/** @relates string */
NONSTATIC_INLINE struct string *
add_to_string(struct string *string, const unsigned char *source)
{
	assertm(string && source, "[add_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	if (!*source) return string;

	return add_bytes_to_string(string, source, strlen(source));
}

/** @relates string */
NONSTATIC_INLINE struct string *
add_crlf_to_string(struct string *string)
{
	assertm(string != NULL, "[add_crlf_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	if (!realloc_string(string, string->length + 2))
		return NULL;

	string->source[string->length++] = ASCII_CR;
	string->source[string->length++] = ASCII_LF;
	string->source[string->length]   = '\0';

	return string;
}

/** @relates string */
NONSTATIC_INLINE struct string *
add_string_to_string(struct string *string, const struct string *from)
{
	assertm(string && from, "[add_string_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);
	check_string_magic(from);

	if (!from->length) return string; /* optimization only */

	return add_bytes_to_string(string, from->source, from->length);
}

/** @relates string */
struct string *
add_file_to_string(struct string *string, const unsigned char *filename)
{
	FILE *file;
	off_t filelen;
	int newlength;

	assertm(string && filename, "[add_file_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	file = fopen(filename, "rb");
	if (!file) return NULL;

	if (fseeko(file, 0, SEEK_END)) goto err;

	filelen = ftello(file);
	if (filelen == -1) goto err;

	if (fseeko(file, 0, SEEK_SET)) goto err;

	newlength = string->length + filelen;
	if (!realloc_string(string, newlength)) goto err;

	string->length += fread(string->source + string->length, 1,
	                        (size_t) filelen, file);
	string->source[string->length] = 0;
	fclose(file);

	if (string->length != newlength) goto err;

	return string;

err:
	fclose(file);

	return NULL;
}

struct string *
string_concat(struct string *string, ...)
{
	va_list ap;
	const unsigned char *source;

	assertm(string != NULL, "[string_concat]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	va_start(ap, string);
	while ((source = va_arg(ap, const unsigned char *)))
		if (*source)
			add_to_string(string, source);

	va_end(ap);

	return string;
}

/** @relates string */
NONSTATIC_INLINE struct string *
add_char_to_string(struct string *string, unsigned char character)
{
	assertm(string && character, "[add_char_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	if (!realloc_string(string, string->length + 1))
		return NULL;

	string->source[string->length++] = character;
	string->source[string->length]   = '\0';

	return string;
}

NONSTATIC_INLINE struct string *
add_xchar_to_string(struct string *string, unsigned char character, int times)
{
	int newlength;

	assertm(string && character && times >= 0, "[add_xchar_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	if (!times) return string;

	newlength = string->length + times;
	if (!realloc_string(string, newlength))
		return NULL;

	memset(string->source + string->length, character, times);
	string->length = newlength;
	string->source[newlength] = '\0';

	return string;
}

/** Add printf()-style format string to @a string. */
struct string *
add_format_to_string(struct string *string, const unsigned char *format, ...)
{
	int newlength;
	int width;
	va_list ap;

	assertm(string && format, "[add_format_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	va_start(ap, format);
	width = vsnprintf(NULL, 0, format, ap);
	va_end(ap);
	if (width <= 0) return NULL;

	newlength = string->length + width;
	if (!realloc_string(string, newlength))
		return NULL;

	va_start(ap, format);
	vsnprintf(&string->source[string->length], width + 1, format, ap);
	va_end(ap);

	string->length = newlength;
	string->source[newlength] = '\0';

	return string;
}

struct string *
add_to_string_list(LIST_OF(struct string_list_item) *list,
		   const unsigned char *source, int length)
{
	struct string_list_item *item;
	struct string *string;

	assertm(list && source, "[add_to_string_list]");
	if_assert_failed return NULL;

	item = mem_alloc(sizeof(*item));
	if (!item) return NULL;

	string = &item->string;
	if (length < 0) length = strlen(source);

	if (!init_string(string)
	    || !add_bytes_to_string(string, source, length)) {
		done_string(string);
		mem_free(item);
		return NULL;
	}

	add_to_list_end(*list, item);
	return string;
}

/** @relates string_list_item */
void
free_string_list(LIST_OF(struct string_list_item) *list)
{
	assertm(list != NULL, "[free_string_list]");
	if_assert_failed return;

	while (!list_empty(*list)) {
		struct string_list_item *item = list->next;

		del_from_list(item);
		done_string(&item->string);
		mem_free(item);
	}
}
