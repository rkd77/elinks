#ifndef EL__UTIL_MEMORY_H
#define EL__UTIL_MEMORY_H

/* If defined, we'll crash if ALLOC_MAXTRIES is attained,
 * if not defined, we'll try to continue. */
/* #define CRASH_IF_ALLOC_MAXTRIES */

/** Max. number of retry in case of memory allocation failure. */
#define ALLOC_MAXTRIES 3

/** Delay in seconds between each alloc try. */
#define ALLOC_DELAY 3

#define fmem_alloc(x) mem_alloc(x)
#define fmem_free(x) mem_free(x)


/** Cygwin wants some size_t definition here... let's try to make it happy
 * then. Hrmpf. */
#include <sys/types.h>
#include <stddef.h>

#ifdef CONFIG_GC
#include <gc.h>
#endif

#ifdef HAVE_MMAP
void *mem_mmap_alloc(size_t size);
void mem_mmap_free(void *p, size_t size);
void *mem_mmap_realloc(void *p, size_t old_size, size_t new_size);
#else
#define mem_mmap_alloc(x) mem_alloc(x)
#define mem_mmap_free(x, y) mem_free(x)
#define mem_mmap_realloc(x, y, z) mem_realloc(x, z)
#endif


#ifdef DEBUG_MEMLEAK

#include "util/memdebug.h"

#define mem_alloc(x) debug_mem_alloc(__FILE__, __LINE__, x)
#define mem_calloc(x, y) debug_mem_calloc(__FILE__, __LINE__, x, y)
#define mem_free(x) debug_mem_free(__FILE__, __LINE__, x)
#define mem_realloc(x, y) debug_mem_realloc(__FILE__, __LINE__, x, y)

#else

#ifndef CONFIG_FASTMEM

void *mem_alloc(size_t);
void *mem_calloc(size_t, size_t);
void mem_free(void *);
void *mem_realloc(void *, size_t);

#else

# include <stdlib.h>

/* TODO: For enhanced portability, checks at configure time:
 * malloc(0) -> NULL
 * realloc(NULL, 0) -> NULL
 * realloc(p, 0) <-> free(p)
 * realloc(NULL, n) <-> malloc(n)
 * Some old implementations may not respect these rules.
 * For these we need some replacement functions.
 * This should not be an issue on most modern systems.
 */
#ifdef CONFIG_GC
# define mem_alloc(size) GC_MALLOC(size)
# define mem_calloc(count, size) GC_MALLOC((count) * (size))
# define mem_free(p) (p) = NULL
# define mem_realloc(p, size) GC_REALLOC(p, size)

#else

# define mem_alloc(size) malloc(size)
# define mem_calloc(count, size) calloc(count, size)
# define mem_free(p) free(p)
# define mem_realloc(p, size) realloc(p, size)

#endif

/* fmem_* functions should be use for allocation and freeing of memory
 * inside a function.
 * See alloca(3) manpage. */

#undef fmem_alloc
#undef fmem_free

#ifdef HAVE_ALLOCA

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#define fmem_alloc(x) alloca(x)
#define fmem_free(x)

#else /* HAVE_ALLOCA */

#define fmem_alloc(x) mem_alloc(x)
#define fmem_free(x) mem_free(x)

#endif /* HAVE_ALLOCA */

#endif /* CONFIG_FASTMEM */

#endif /* DEBUG_MEMLEAK */


/** @name Granular memory allocation.
 * The granularity used by the aligned memory functions below must be a mask
 * with all bits set from but not including the most significant bit and down.
 * So if an alignment of 256 is wanted use 0xFF.
 * @{ */

/** The 'old' style granularity. XXX: Must be power of 2 */
#define ALLOC_GR 0x100

#include <string.h> /* for memset() */

#define ALIGN_MEMORY_SIZE(x, gr) (((x) + (gr)) & ~(gr))

static inline void *
mem_align_alloc__(
#ifdef DEBUG_MEMLEAK
		  const unsigned char *file, int line,
#endif
		  void **ptr, size_t old, size_t new, size_t objsize, size_t mask)
{
	size_t newsize = ALIGN_MEMORY_SIZE(new, mask);
	size_t oldsize = ALIGN_MEMORY_SIZE(old, mask);

	if (newsize > oldsize) {
		unsigned char *data;

		newsize *= objsize;
		oldsize *= objsize;

#ifdef DEBUG_MEMLEAK
		data = debug_mem_realloc(file, line, *ptr, newsize);
#else
		data = mem_realloc(*ptr, newsize);
#endif
		if (!data) return NULL;

		*ptr = (void *) data;
		memset(&data[oldsize], 0, newsize - oldsize);
	}

	return *ptr;
}

#ifdef DEBUG_MEMLEAK
#define mem_align_alloc(ptr, old, new, mask) \
	mem_align_alloc__(__FILE__, __LINE__, (void **) ptr, old, new, sizeof(**ptr), mask)
#else
#define mem_align_alloc(ptr, old, new, mask) \
	mem_align_alloc__((void **) ptr, old, new, sizeof(**ptr), mask)
#endif

/** @} */


/** @name Maybe-free macros
 * @todo TODO: Think about making what they do more obvious in their
 * identifier, they could be obfuscating their users a little for the
 * newcomers otherwise.
 * @{ */

#define mem_free_set(x, v) do { if (*(x)) mem_free(*(x)); *(x) = (v); } while (0)
#define mem_free_if(x) do { register void *p = (x); if (p) mem_free(p); } while (0)

#if 0
/* This may help to find bugs. */
#undef mem_free_if
#define mem_free_if(x) mem_free_set(&x, NULL)
#endif
/** @} */


/* This is out of place, but there is no better place. */

#ifdef DEBUG_MEMLEAK
#define intdup(i) intdup__(__FILE__, __LINE__, i)
#else
#define intdup(i) intdup__(i)
#endif

static inline int *
intdup__(
#ifdef DEBUG_MEMLEAK
         unsigned char *file, int line,
#endif
         int i)
{
#ifdef DEBUG_MEMLEAK
	int *p = debug_mem_alloc(file, line, sizeof(*p));
#else
	int *p = mem_alloc(sizeof(*p));
#endif

	if (p) *p = i;

	return p;
}

#endif
