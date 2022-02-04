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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DEBUG_MEMLEAK

/** @name Autoallocation string constructors:
 * Note that, contrary to the utilities using the string struct, these
 * functions are NOT granular, thus you can't simply reuse strings allocated by
 * these in add_to_string()-style functions.
 * @{ */

/** Allocates NUL terminated string with @a len bytes from @a src.
 * If @a src == NULL or @a len < 0 only one byte is allocated and set it to 0.
 * @returns the string or NULL on allocation failure. */
char *memacpy(const char *src, int len);

/** Allocated NUL terminated string with the content of @a src. */
char *stracpy(const char *src);

/** @} */

#else /* DEBUG_MEMLEAK */

char *debug_memacpy(const char *, int, const char *, int);
#define memacpy(s, l) debug_memacpy(__FILE__, __LINE__, s, l)

char *debug_stracpy(const char *, int, const char *);
#define stracpy(s) debug_stracpy(__FILE__, __LINE__, s)

#endif /* DEBUG_MEMLEAK */


/** Concatenates @a src to @a str.
 * If reallocation of @a str fails @a str is not touched. */
void add_to_strn(char **str, const char *src);

/** Inserts @a seqlen chars from @a seq at position @a pos in the @a
 * dst string.
 * If reallocation of @a dst fails it is not touched and NULL is returned. */
char *insert_in_string(char **dst, int pos,
				const char *seq, int seqlen);

/** Takes a list of strings where the last parameter _must_ be
 * (char *) NULL and concatenates them.
 *
 * @returns the allocated string or NULL on allocation failure.
 *
 * Example: @code
 *	char *abc = straconcat("A", "B", "C", (char *) NULL);
 *	if (abc) return;
 *	printf("%s", abc);	-> print "ABC"
 *	mem_free(abc);		-> free memory used by @abc
 * @endcode */
char *straconcat(const char *str, ...);


/** @name Misc. utility string functions.
 * @{ */

/** Compare two strings, handling correctly @a s1 or @a s2 being NULL. */
int xstrcmp(const char *s1, const char *s2);

/** Copies at most @a len chars into @a dst. Ensures null termination of @a dst. */
char *safe_strncpy(char *dst, const char *src, size_t len);

/* strlcmp() is the middle child of history, everyone is using it differently.
 * On some weird *systems* it seems to be defined (equivalent to strcasecmp()),
 * so we'll better use our #define redir. */

/** This routine compares string @a s1 of length @a n1 with string @a s2
 * of length @a n2.
 *
 * This acts identically to strcmp() but for non-zero-terminated strings,
 * rather than being similiar to strncmp(). That means, it fails if @a n1
 * != @a n2, thus you may use it for testing whether @a s2 matches *full*
 * @a s1, not only its start (which can be a security hole, e.g. in the
 * cookies domain checking).
 *
 * @a n1 or @a n2 may be -1, which is same as strlen(@a s1 or @a s2) but
 * possibly more effective (in the future ;-).
 *
 * @returns zero if the strings match or undefined non-zero value if they
 * differ.  (The non-zero return value is _not_ same as for the standard
 * strcmp() family.) */
#define strlcmp(a,b,c,d) (errfile = __FILE__, errline = __LINE__, elinks_strlcmp(a,b,c,d))
int elinks_strlcmp(const char *s1, size_t n1,
		   const char *s2, size_t n2);

/** Acts identically to strlcmp(), except for being case insensitive. */
#define strlcasecmp(a,b,c,d) (errfile = __FILE__, errline = __LINE__, elinks_strlcasecmp(a,b,c,d,0))
#define c_strlcasecmp(a,b,c,d) (errfile = __FILE__, errline = __LINE__, elinks_strlcasecmp(a,b,c,d,1))
int elinks_strlcasecmp(const char *s1, size_t n1,
		       const char *s2, size_t n2,
		       const int locale_indep);

#define strlcasestr(a,b,c,d) (errfile = __FILE__, errline = __LINE__, elinks_strlcasestr(a,b,c,d))
char *elinks_strlcasestr(const char *haystack, const int haystackl,
                         const char *needle, const int needlel);

/* strcasecmp and strncasecmp which work as if they are
 * in the C locale */
int c_strcasecmp(const char *s1, const char *s2);
int c_strncasecmp(const char *s1, const char *s2, size_t n);

/* strcasestr function which works as if it is in the C locale. */
char * c_strcasestr(const char *haystack, const char *needle);

/** @} */


#define skip_space(S) \
	do { while (isspace(*(S))) (S)++; } while (0)

#define skip_nonspace(S) \
	do { while (*(S) && !isspace(*(S))) (S)++; } while (0)

#undef isdigit
#define isdigit(c)	((c) >= '0' && (c) <= '9')
#define isquote(c)	((c) == '"' || (c) == '\'')
#define isasciialpha(c)	(((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#define isasciialnum(c)	(isasciialpha(c) || isdigit(c))
#define isident(c)	(isasciialnum(c) || (c) == '_' || (c) == '-')

/** Char is safe to write to the terminal screen.  Cannot test for C1
 * control characters (0x80 to 0x9F) because this is also used for
 * non-ISO-8859 charsets.  */
#define isscreensafe(c)	((c) >= ' ' && (c) != ASCII_DEL)

/** Like isscreensafe() but takes Unicode values and so can check for C1.  */
#define isscreensafe_ucs(c) (((c) >= 0x20 && (c) <= 0x7E) || (c) >= 0xA0)


/** String debugging using magic number, it may catch some errors. */
#ifdef CONFIG_DEBUG
#define DEBUG_STRING
#endif

struct string {
#ifdef DEBUG_STRING
	int magic;
#endif
	union {
		const char *csource;
		char *source;
	};
	int length;
};


/** The granularity used for the struct string based utilities. */
#define STRING_GRANULARITY 0xFF

#ifdef DEBUG_STRING
#define STRING_MAGIC 0x2E5BF271
#define check_string_magic(x) assertm((x)->magic == STRING_MAGIC, "String magic check failed.")
#define set_string_magic(x) do { (x)->magic = STRING_MAGIC; } while (0)
#define NULL_STRING { STRING_MAGIC, {NULL}, 0 }
#define INIT_STRING(s, l) { STRING_MAGIC, {s}, l }
#else
#define check_string_magic(x)
#define set_string_magic(x)
#define NULL_STRING { {NULL}, 0 }
#define INIT_STRING(s, l) { {s}, l }
#endif

/** Initializes the passed string struct by preallocating the
 * string.source member.
 * @returns @a string if successful, or NULL if out of memory.
 * @post done_string(@a string) is safe, even if this returned NULL.
 * @relates string */
#ifdef __cplusplus
#ifdef DEBUG_MEMLEAK
[[nodiscard]] struct string *init_string__(const char *file, int line, struct string *string);
#define init_string(string) init_string__(__FILE__, __LINE__, string)
#else
[[nodiscard]] struct string *init_string(struct string *string);
#endif

#else

#ifdef DEBUG_MEMLEAK
struct string *init_string__(const char *file, int line, struct string *string);
#define init_string(string) init_string__(__FILE__, __LINE__, string)
#else
struct string *init_string(struct string *string);
#endif

#endif
/** Resets @a string and free()s the string.source member.
 * @relates string */
void done_string(struct string *string);


struct string *add_to_string(struct string *string,
			     const char *source);
struct string *add_char_to_string(struct string *string, unsigned char character);
struct string *add_string_to_string(struct string *to, const struct string *from);
struct string *add_file_to_string(struct string *string, const char *filename);
struct string *add_crlf_to_string(struct string *string);

/** Adds each C string to @a string until a terminating
 * (char *) NULL is met.
 * @relates string */
struct string *string_concat(struct string *string, ...);

/** Extends the string with @a times number of @a character.
 * @relates string */
struct string *add_xchar_to_string(struct string *string, unsigned char character, int times);

/** Add printf()-style format string to @a string.
 * @relates string */
struct string *add_format_to_string(struct string *string, const char *format, ...);

/** Get a regular newly allocated stream of bytes from @a string.
 * @relates string */
static char *squeezastring(struct string *string);


static inline char *
squeezastring(struct string *string)
{
	return memacpy(string->source, string->length);
}


#undef realloc_string

#define realloc_string(str, size) \
	mem_align_alloc(&(str)->source, (str)->length, (size) + 1, \
			STRING_GRANULARITY)

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
		    const char *file, int line,
#endif
		    struct string *string, const char *bytes,
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

/** Adds @a string with @a length chars to the list. If @a length is -1
 * it will be set to the return value of strlen().
 * @relates string_list_item */
struct string *
add_to_string_list(LIST_OF(struct string_list_item) *list,
		   const char *string, int length);

void free_string_list(LIST_OF(struct string_list_item) *list);

void string_replace(struct string *res, struct string *inp, struct string *what, struct string *repl);


/** Returns an empty C string or @a str if different from NULL. */
#define empty_string_or_(str) ((str) ? (char *) (str) : (char *) "")

/** Allocated copy if not NULL or returns NULL. */
#define null_or_stracpy(str) ((str) ? stracpy(str) : NULL)

#ifdef __cplusplus
}
#endif

#endif /* EL__UTIL_STRING_H */
