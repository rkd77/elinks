#ifndef EL__UTIL_FASTFIND_H
#define EL__UTIL_FASTFIND_H

/** Whether to use these routines or not. */
#ifndef CONFIG_SMALL
#define USE_FASTFIND 1
#else
#undef USE_FASTFIND
#endif

#ifdef USE_FASTFIND

struct fastfind_key_value {
	unsigned char *key;
	void *data;
};

enum fastfind_flags {
	FF_NONE = 0,
	FF_CASE_AWARE = 1,	/**< honour case when comparing */
	FF_COMPRESS = 2,	/**< compress nodes if possible */
	FF_LOCALE_INDEP = 4	/**< whether the case conversion is
				 *   locale independent or not */
};

struct fastfind_index {
	/** Description useful for debugging mode. */
	unsigned char *comment;
	/** Start over. */
	void (*reset)(void);
	/** Get next struct fastfind_key_value in line. */
	struct fastfind_key_value *(*next)(void);
	/** Internal reference */
	void *handle;
};

#define INIT_FASTFIND_INDEX(comment, reset, next) \
	{ (comment), (reset), (next) }

/** Initialize and index a list of keys.
 * Keys are iterated using:
 * @param index		index info
 * @param flags 	control case sensitivity, compression
 *
 * This function must be called once and only once per list.
 * Failure is not an option, so call it at startup.
 * @relates fastfind_index */
struct fastfind_index *fastfind_index(struct fastfind_index *index, enum fastfind_flags flags);

/* The main reason of all that stuff is here. */
/** Search the index for @a key with length @a key_len using the
 * @a index' handle created with fastfind_index().
 * @relates fastfind_index */
void *fastfind_search(struct fastfind_index *index,
		      const unsigned char *key, int key_len);

/** Fastfind cleanup. It frees the given @a index.
 * Must be called once per list.
 * @relates fastfind_index */
void fastfind_done(struct fastfind_index *index);

#endif

#endif /* EL__UTIL_FASTFIND_H */
