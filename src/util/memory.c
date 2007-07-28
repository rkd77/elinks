/** Memory allocation manager
 * @file */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* MREMAP_MAYMOVE */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#include <unistd.h>

#include "elinks.h"

#include "util/error.h"
#include "util/memory.h"


#if !defined(DEBUG_MEMLEAK) && !defined(CONFIG_FASTMEM)

static int alloc_try = 0;

static int
patience(unsigned char *of)
{
	++alloc_try;
	if (alloc_try < ALLOC_MAXTRIES) {
		ERROR("Out of memory (%s returned NULL): retry #%d/%d, "
		      "I still exercise my patience and retry tirelessly.",
		      of, alloc_try, ALLOC_MAXTRIES);
		sleep(ALLOC_DELAY);
		return alloc_try;
	}

#ifdef CRASH_IF_ALLOC_MAXTRIES
	INTERNAL("Out of memory (%s returned NULL) after %d tries, "
		 "I give up. See ya on the other side.",
		 of, alloc_try);
#else
	ERROR("Out of memory (%s returned NULL) after %d tries, "
	      "I give up and try to continue. Pray for me, please.",
	      of, alloc_try);
#endif
	alloc_try = 0;
	return 0;
}

void *
mem_alloc(size_t size)
{
	if (size)
		do {
#ifdef CONFIG_GC
			void *p = GC_MALLOC(size);
#else
			void *p = malloc(size);
#endif
			if (p) return p;
		} while (patience("malloc"));

	return NULL;
}

void *
mem_calloc(size_t count, size_t eltsize)
{
	if (eltsize && count)
		do {
#ifdef CONFIG_GC
			void *p = GC_MALLOC(count * eltsize);
#else
			void *p = calloc(count, eltsize);
#endif
			if (p) return p;
		} while (patience("calloc"));

	return NULL;
}

void
mem_free(void *p)
{
	if (!p) {
		INTERNAL("mem_free(NULL)");
		return;
	}
#ifdef CONFIG_GC
	p = NULL;
#else
	free(p);
#endif
}

void *
mem_realloc(void *p, size_t size)
{
	if (!p) return mem_alloc(size);

	if (size)
		do {
#ifdef CONFIG_GC
			void *p2 = GC_REALLOC(p, size);
#else
			void *p2 = realloc(p, size);
#endif
			if (p2) return p2;
		} while (patience("realloc"));
	else
		mem_free(p);

	return NULL;
}

#endif


/* TODO: Leak detector and the usual protection gear? patience()?
 *
 * We could just alias mem_mmap_* to mem_debug_* #if DEBUG_MEMLEAK, *WHEN* we are
 * confident that the mmap() code is really bugless ;-). --pasky */

#ifdef HAVE_MMAP

static int page_size;

/** Round up to a full page.
 * This tries to prevent useless reallocations, especially since they
 * are quite expensive in the mremap()-less case. */
static size_t
round_size(size_t size)
{
#ifdef HAVE_SC_PAGE_SIZE
	if (!page_size) page_size = sysconf(_SC_PAGE_SIZE);
#endif
	if (page_size <= 0) page_size = 1;
	return (size / page_size + 1) * page_size;
}

/** Some systems may not have MAP_ANON but MAP_ANONYMOUS instead. */
#if defined(MAP_ANONYMOUS) && !defined(MAP_ANON)
#define MAP_ANON MAP_ANONYMOUS
#endif

void *
mem_mmap_alloc(size_t size)
{
	if (size) {
		void *p = mmap(NULL, round_size(size), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

		if (p != MAP_FAILED)
			return p;
	}

	return NULL;
}

void
mem_mmap_free(void *p, size_t size)
{
	if (!p) {
		INTERNAL("mem_mmap_free(NULL)");
		return;
	}

	munmap(p, round_size(size));
}

void *
mem_mmap_realloc(void *p, size_t old_size, size_t new_size)
{
	if (!p) return mem_mmap_alloc(new_size);

	if (round_size(old_size) == round_size(new_size))
		return p;

	if (new_size) {
#ifdef HAVE_MREMAP
		void *p2 = mremap(p, round_size(old_size), round_size(new_size), MREMAP_MAYMOVE);

		if (p2 != MAP_FAILED)
			return p2;
#else
		void *p2 = mem_mmap_alloc(new_size);

		if (p2) {
			memcpy(p2, p, MIN(old_size, new_size));
			mem_mmap_free(p, old_size);
			return p2;
		}
#endif
	} else {
		mem_mmap_free(p, old_size);
	}

	return NULL;
}

#endif
