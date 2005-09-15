/* $Id: string.h,v 1.92 2005/01/03 07:41:01 miciah Exp $ */

#ifndef EL__UTIL_STRING_H
#define EL__UTIL_STRING_H

/* To these two functions, same remark applies as to copy_string() or
 * straconcat(). */

#include <ctype.h>
#include <string.h>

#include "osdep/ascii.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memdebug.h"
#include "util/memory.h"


#ifndef DEBUG_MEMLEAK

/* Autoallocation string constructors: */

/* Note that, contrary to the utilities using the string struct, these
 * functions are NOT granular, thus you can't simply reuse strings allocated by
 * these in add_to_string()-style functions. */

/* Allocates NUL terminated string with @len bytes from @src.
 * If @src == NULL or @len < 0 only one byte is allocated and set it to 0. */
/* Returns the string or NULL on allocation failure. */
unsigned char *memacpy(unsigned char *src, int len);

/* Allocated NUL terminated string with the content of @src. */
unsigned char *stracpy(unsigned char *src);

#else /* DEBUG_MEMLEAK */

unsigned char *debug_memacpy(unsigned char *, int, unsigned char *, int);
#define memacpy(s, l) debug_memacpy(__FILE__, __LINE__, s, l)

unsigned char *debug_stracpy(unsigned char *, int, unsigned char *);
#define stracpy(s) debug_stracpy(__FILE__, __LINE__, s)

#endif /* DEBUG_MEMLEAK */


/* Concatenates @src to @str. */
/* If reallocation of @str fails @str is not touched. */
void add_to_strn(unsigned char **str, unsigned char *src);

/* Inserts @seqlen chars from @seq at position @pos in the @dst string. */
/* If reallocation of @dst fails it is not touched and NULL is returned. */
unsigned char *
insert_in_string(unsigned char **dst, int pos, unsigned char *seq, int seqlen);

/* Takes a list of strings where the last parameter _must_ be NULL and
 * concatenates them. */
/* Returns the allocated string or NULL on allocation failure. */
/* Example:
 *	unsigned char *abc = straconcat("A", "B", "C", NULL);
 *	if (abc) return;
 *	printf("%s", abc);	-> print "ABC"
 *	mem_free(abc);		-> free memory used by @abc */
unsigned char *straconcat(unsigned char *str, ...);


/* Misc. utility string functions. */

/* Compare two strings, handling correctly @s1 or @s2 being NULL. */
int xstrcmp(unsigned char *s1, unsigned char *s2);

/* Copies at most @len chars into @dst. Ensures null termination of @dst. */
unsigned char *safe_strncpy(unsigned char *dst, const unsigned char *src, size_t len);

/* strlcmp() is the middle child of history, everyone is using it differently.
 * On some weird *systems* it seems to be defined (equivalent to strcasecmp()),
 * so we'll better use our #define redir. */

/* This routine compares string @s1 of length @n1 with string @s2 of length
 * @n2.
 *
 * This acts identically to strcmp() but for non-zero-terminated strings,
 * rather than being similiar to strncmp(). That means, it fails if @n1 != @n2,
 * thus you may use it for testing whether @s2 matches *full* @s1, not only its
 * start (which can be a security hole, ie. in the cookies domain checking).
 *
 * @n1 or @n2 may be -1, which is same as strlen(@s[12]) but possibly more
 * effective (in the future ;-). */
/* Returns zero if the strings match or undefined non-zero value if they
 * differ.  (The non-zero return value is _not_ same as for the standard
 * strcmp() family.) */
#define strlcmp(a,b,c,d) (errfile = __FILE__, errline = __LINE__, elinks_strlcmp(a,b,c,d))
int elinks_strlcmp(const unsigned char *s1, size_t n1,
		   const unsigned char *s2, size_t n2);

/* Acts identically to strlcmp(), except for being case insensitive. */
#define strlcasecmp(a,b,c,d) (errfile = __FILE__, errline = __LINE__, elinks_strlcasecmp(a,b,c,d))
int elinks_strlcasecmp(const unsigned char *s1, size_t n1,
		       const unsigned char *s2, size_t n2);


#define skip_space(S) \
	do { while (isspace(*(S))) (S)++; } while (0)

#define skip_nonspace(S) \
	do { while (*(S) && !isspace(*(S))) (S)++; } while (0)

#define isquote(c)	((c) == '"' || (c) == '\'')
#define isasciialpha(c)	(((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#define isasciialnum(c)	(isasciialpha(c) || isdigit(c))
#define isident(c)	(isasciialnum(c) || (c) == '_' || (c) == '-')

/* Char is safe to write to the terminal screen */
#define isscreensafe(c)	((c) >= ' ' && (c) != ASCII_DEL)


/* String debugging using magic number, it may catch some errors. */
#ifdef CONFIG_DEBUG
#define DEBUG_STRING
#endif

struct string {
#ifdef DEBUG_STRING
	int magic;
#endif
	unsigned char *source;
	int length;
};


/* The granularity used for the struct string based utilities. */
#define STRING_GRANULARITY 0xFF

#ifdef DEBUG_STRING
#define STRING_MAGIC 0x2E5BF271
#define check_string_magic(x) assertm((x)->magic == STRING_MAGIC, "String magic check failed.")
#define set_string_magic(x) do { (x)->magic = STRING_MAGIC; } while (0)
#define NULL_STRING { STRING_MAGIC, NULL, 0 }
#define INIT_STRING(s, l) { STRING_MAGIC, s, l }
#else
#define check_string_magic(x)
#define set_string_magic(x)
#define NULL_STRING { NULL, 0 }
#define INIT_STRING(s, l) { s, l }
#endif

/* Initializes the passed string struct by preallocating the @source member. */
#ifdef DEBUG_MEMLEAK
struct string *init_string__(unsigned char *file, int line, struct string *string);
#define init_string(string) init_string__(__FILE__, __LINE__, string)
#else
struct string *init_string(struct string *string);
#endif

/* Resets @string and free()s the @source member. */
void done_string(struct string *string);


struct string *add_to_string(struct string *string,
			     const unsigned char *source);
struct string *add_char_to_string(struct string *string, unsigned char character);
struct string *add_string_to_string(struct string *to, struct string *from);
struct string *add_crlf_to_string(struct string *string);

/* Adds each C string to @string until a terminating NULL is met. */
struct string *string_concat(struct string *string, ...);

/* Extends the string with @times number of @character. */
struct string *add_xchar_to_string(struct string *string, unsigned char character, int times);

/* Add printf-style format string to @string. */
struct string *add_format_to_string(struct string *string, unsigned char *format, ...);

/* Get a regular newly allocated stream of bytes from @string. */
static unsigned char *squeezastring(struct string *string);


static inline unsigned char *
squeezastring(struct string *string)
{
	return memacpy(string->source, string->length);
}


#undef realloc_string

#define realloc_string(str, size) \
	mem_align_alloc(&(str)->source, (str)->length, (size) + 1, \
			unsigned char, STRING_GRANULARITY)

#ifdef DEBUG_MEMLEAK

#define add_bytes_to_string(string, bytes, length) \
	add_bytes_to_string__(__FILE__, __LINE__, string, bytes, length)

#define debug_realloc_string(str, size) \
	mem_align_alloc__(file, line, (void **) &(str)->source, (str)->length, (size) + 1, \
			  sizeof(unsigned char), STRING_GRANULARITY)

#else

#define add_bytes_to_string(string, bytes, length) \
	add_bytes_to_string__(string, bytes, length)

#define debug_realloc_string(str, size) realloc_string(str, size)

#endif

static inline struct string *
add_bytes_to_string__(
#ifdef DEBUG_MEMLEAK
		    unsigned char *file, int line,
#endif
		    struct string *string, const unsigned char *bytes,
		    int length)
{
	int newlength;

	assertm(string && bytes && length >= 0, "[add_bytes_to_string]");
	if_assert_failed { return NULL; }

	check_string_magic(string);

	if (length == 0) return string;

	newlength = string->length + length;
	if (!debug_realloc_string(string, newlength))
		return NULL;

	memcpy(string->source + string->length, bytes, length);
	string->source[newlength] = 0;
	string->length = newlength;

	return string;
}


struct string_list_item {
	LIST_HEAD(struct string_list_item);

	struct string string;
};

/* Adds @string with @length chars to the list. If length is -1 it will be set
 * to the return value of strlen(). */
struct string *
add_to_string_list(struct list_head *list, const unsigned char *string,
		   int length);

void free_string_list(struct list_head *list);


/* Returns an empty C string or @str if different from NULL. */
#define empty_string_or_(str) ((str) ? (unsigned char *) (str) : (unsigned char *) "")

/* Allocated copy if not NULL or returns NULL. */
#define null_or_stracpy(str) ((str) ? stracpy(str) : NULL)

#endif /* EL__UTIL_STRING_H */
