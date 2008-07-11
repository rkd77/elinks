#ifndef EL__CACHE_CACHE_H
#define EL__CACHE_CACHE_H

#include "main/object.h"
#include "util/lists.h"
#include "util/time.h"

struct listbox_item;
struct uri;

/* This enum describes the level of caching of certain cache entry. That is,
 * under what conditions shall it be reloaded, if ever. The one with lowest
 * value is most agressively cached, however the cache is most reluctant to
 * cache the one with highest value. */
/* TODO: Invert order to be more intuitive. But be careful, many tests rely on
 * current order. --Miciah,Zas */
enum cache_mode {
	CACHE_MODE_INCREMENT = -1,
	CACHE_MODE_ALWAYS,
	CACHE_MODE_NORMAL,
	CACHE_MODE_CHECK_IF_MODIFIED,
	CACHE_MODE_FORCE_RELOAD,
	CACHE_MODE_NEVER,
};

struct cache_entry {
	OBJECT_HEAD(struct cache_entry);

	/* Items in this list are ALLOCATED IN A NON-STANDARD WAY! Thus if you
	 * are gonna mess with them (you shouldn't), you need to use the
	 * mmap suite. */
	LIST_OF(struct fragment) frag;

	struct uri *uri;		/* Identifier for the cached data */
	struct uri *proxy_uri;		/* Proxy identifier or same as @uri */
	struct uri *redirect;		/* Location we were redirected to */

	unsigned char *head;		/* The protocol header */
	unsigned char *content_type;	/* MIME type: <type> "/" <subtype> */
	unsigned char *last_modified;	/* Latest modification date */
	unsigned char *etag;		/* ETag value from the HTTP header */
	unsigned char *ssl_info;	/* SSL ciphers used during transfer */
	unsigned char *encoding_info;	/* Encoding used during transfer */

	unsigned int cache_id;		/* Change each time entry is modified. */

	time_t seconds;			/* Access time. Used by 'If-Modified-Since' */

	off_t length;			/* The expected and complete size */
	off_t data_size;		/* The actual size of all fragments */

	struct listbox_item *box_item;	/* Dialog data for cache manager */
#ifdef CONFIG_SCRIPTING_SPIDERMONKEY
	struct JSObject *jsobject;      /* Instance of cache_entry_class */
#endif

	timeval_T max_age;		/* Expiration time */

	unsigned int expire:1;		/* Whether to honour max_age */
	unsigned int preformatted:1;	/* Has content been preformatted? */
	unsigned int redirect_get:1;	/* Follow redirect using get method? */
	unsigned int incomplete:1;	/* Has all data been downloaded? */
	unsigned int valid:1;		/* Is cache entry usable? */

	/* This is a mark for internal workings of garbage_collection(), whether
	 * the cache_entry should be busted or not. You are not likely to see
	 * an entry with this set to 1 in wild nature ;-). */
	unsigned int gc_target:1;	/* The GC touch of death */
	unsigned int cgi:1;		/* Is a CGI output? */

	enum cache_mode cache_mode;	/* Reload condition */
};

struct fragment {
	LIST_HEAD(struct fragment);

	off_t offset;
	off_t length;
	off_t real_length;
	unsigned char data[1]; /* Must be last */
};


/* Searches the cache for an entry matching the URI. Returns NULL if no one
 * matches. */
struct cache_entry *find_in_cache(struct uri *uri);

/* Searches the cache for a matching entry else a new one is added. Returns
 * NULL if allocation fails. */
struct cache_entry *get_cache_entry(struct uri *uri);

/* Searches the cache for a matching entry and checks if it is still valid and
 * usable. Returns NULL if the @cache_mode suggests to reload it again. */
struct cache_entry *get_validated_cache_entry(struct uri *uri, enum cache_mode cache_mode);

/* Checks if a dangling cache entry pointer is still valid. */
int cache_entry_is_valid(struct cache_entry *cached);

/* Follow all redirects and return the resulting cache entry or NULL if there
 * are missing redirects. */
struct cache_entry *follow_cached_redirects(struct cache_entry *cached);

/* Works like find_in_cache(), but will follow cached redirects using
 * follow_cached_redirects(). */
struct cache_entry *get_redirected_cache_entry(struct uri *uri);

/* Add a fragment to the @cached object at the given @offset containing @length
 * bytes from the @data pointer. */
/* Returns -1 upon error,
 *	    1 if cache entry was enlarged,
 *	    0 if only old data were overwritten. */
int add_fragment(struct cache_entry *cached, off_t offset,
		 const unsigned char *data, ssize_t length);

/* Defragments the cache entry and returns the resulting fragment containing the
 * complete source of all currently downloaded fragments. Returns NULL if
 * validation of the fragments fails. */
struct fragment *get_cache_fragment(struct cache_entry *cached);

/* Should be called when creation of a new cache has been completed. Most
 * importantly, it will updates cached->incomplete. */
void normalize_cache_entry(struct cache_entry *cached, off_t length);

void free_entry_to(struct cache_entry *cached, off_t offset);
void delete_entry_content(struct cache_entry *cached);
void delete_cache_entry(struct cache_entry *cached);

/* Sets up the cache entry to redirect to a new location
 * @location	decides where to redirect to by resolving it relative to the
 *		entry's URI.
 * @get		controls the method should be used when handling the redirect.
 * @incomplete	will become the new value of the incomplete member if it
 *		is >= 0.
 * Returns the URI being redirected to or NULL if allocation failed.
 */
struct uri *
redirect_cache(struct cache_entry *cached, unsigned char *location,
		int get, int incomplete);

/* The garbage collector trigger. If @whole is zero, remove unused cache
 * entries which are bigger than the cache size limit set by user. For @zero
 * being one, remove all unused cache entries. */
void garbage_collection(int whole);

/* Used by the resource and memory info dialogs for getting information about
 * the cache. */
unsigned longlong get_cache_size(void);
int get_cache_entry_count(void);
int get_cache_entry_used_count(void);
int get_cache_entry_loading_count(void);

#endif
