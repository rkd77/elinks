#ifndef EL__UTIL_MEMDEBUG_H
#define EL__UTIL_MEMDEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG_MEMLEAK

/* TODO: Another file? */

struct mem_stats {
	long true_amount;
	long amount;
};

extern struct mem_stats mem_stats;

void *debug_mem_alloc(const char *, int, size_t);
void *debug_mem_calloc(const char *, int, size_t, size_t);
void debug_mem_free(const char *, int, void *);
void *debug_mem_realloc(const char *, int, void *, size_t);
void set_mem_comment(void *, const char *, int);

void check_memory_leaks(void);

#else
#define set_mem_comment(p, c, l)
#endif /* DEBUG_MEMLEAK */

#ifdef __cplusplus
}
#endif

#endif
