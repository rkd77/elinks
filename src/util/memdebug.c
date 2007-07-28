/** Memory debugging (leaks, overflows & co)
 * @file
 *
 * Wrappers for libc memory managment providing protection against common
 * pointers manipulation mistakes - bad realloc()/free() pointers, double
 * free() problem, using uninitialized/freed memory, underflow/overflow
 * protection, leaks tracking...
 *
 * Copyright (C) 1999 - 2002  Mikulas Patocka
 * Copyright (C) 2001 - 2004  Petr Baudis
 * Copyright (C) 2002 - 2003  Laurent Monin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * This file is covered by the General Public Licence v2. */

/* This file is very useful even for projects other than ELinks and I like to
 * refer to it through its cvsweb URL, therefore it is a good thing to include
 * the full copyright header here, contrary to the usual ELinks customs. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "util/error.h"
#include "util/lists.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"


#ifdef DEBUG_MEMLEAK

/** Eat less memory, but sacrifice speed?
 * Default is defined. */
#define LESS_MEMORY_SPEED

/** Fill memory on alloc() ?
 * Default is defined. */
#define FILL_ON_ALLOC
#define FILL_ON_ALLOC_VALUE 'X'

/** Fill memory on realloc() ?
 * Default is defined. */
#define FILL_ON_REALLOC
#define FILL_ON_REALLOC_VALUE 'Y'

/** Fill memory before free() ?
 * Default is undef. */
#undef FILL_ON_FREE
#define FILL_ON_FREE_VALUE 'Z'

/** Check alloc_header block sanity ?
 * Default is defined. */
#define CHECK_AH_SANITY
#define AH_SANITY_MAGIC 0xD3BA110C

/** Check for useless reallocation ?
 * If oldsize is equal to newsize, print a message to stderr.
 * It may help to find inefficient code.
 * Default is undefined.
 */
#undef CHECK_USELESS_REALLOC

/** Check for validity of address passed to free() ?
 * Note that this is VERY slow, as we iterate through whole memory_list each
 * time. We can't check magics etc, as it would break double free() check.
 * Default is undef. */
#undef CHECK_INVALID_FREE

/** Check for double free ?
 * Default is defined. */
#define CHECK_DOUBLE_FREE
#define AH_FREE_MAGIC 0xD3BF110C

/** Check for overflows and underflows ?
 * Default is defined. */
#define CHECK_XFLOWS
#define XFLOW_MAGIC (char) 0xFA

/* Log all (de-)allocations to stderr (huge and slow)
 * Default is undefined. */
/* #define LOG_MEMORY_ALLOC */

/* --------- end of debugger configuration section */


struct alloc_header {
	LIST_HEAD(struct alloc_header);

#ifdef CHECK_AH_SANITY
	int magic;
#endif
	int size;
	int line;
	const unsigned char *file;
	unsigned char *comment;

#ifdef CHECK_XFLOWS
	/** This is a little magic. We want to keep the main pointer aligned,
	 * that means we want to have the xflow underflow mark in the
	 * alloc_header space, but at the _end_ of the aligned reserved space.
	 * This means we in fact live at [SIZE_AH_ALIGNED - 1], not here. (Of
	 * course this might be equivalent in some cases, but it is very
	 * unlikely in practice.) */
	unsigned char xflow_underflow_placeholder;
#endif
};

/* Size is set to be on boundary of 8 (a multiple of 7) in order to have the
 * main ptr aligned properly (faster access). We hope that  */
#ifdef LESS_MEMORY_SPEED
#define SIZE_AH_ALIGNED ((sizeof(struct alloc_header) + 7) & ~7)
#else
/* Especially on 128bit machines, this can be faster, but eats more memory. */
#define SIZE_AH_ALIGNED ((sizeof(struct alloc_header) + 15) & ~15)
#endif

#ifdef CHECK_XFLOWS
#define XFLOW_INC 1
#else
#define XFLOW_INC 0
#endif

/* These macros are used to convert pointers and sizes to or from real ones
 * when using alloc_header stuff. */
#define PTR_AH2BASE(ah) (void *) ((char *) (ah) + SIZE_AH_ALIGNED)
#define PTR_BASE2AH(ptr) (struct alloc_header *) \
				((char *) (ptr) - SIZE_AH_ALIGNED)

/* The second overflow mark is not embraced in SIZE_AH_ALIGNED. */
#define SIZE_BASE2AH(size) ((size) + SIZE_AH_ALIGNED + XFLOW_INC)
#define SIZE_AH2BASE(size) ((size) - SIZE_AH_ALIGNED - XFLOW_INC)

#ifdef CHECK_XFLOWS
#define PTR_OVERFLOW_MAGIC(ah) ((char *) PTR_AH2BASE(ah) + (ah)->size)
#define PTR_UNDERFLOW_MAGIC(ah) ((char *) PTR_AH2BASE(ah) - 1)
#define SET_OVERFLOW_MAGIC(ah) (*PTR_OVERFLOW_MAGIC(ah) = XFLOW_MAGIC)
#define SET_UNDERFLOW_MAGIC(ah) (*PTR_UNDERFLOW_MAGIC(ah) = XFLOW_MAGIC)
#define SET_XFLOW_MAGIC(ah) SET_OVERFLOW_MAGIC(ah), SET_UNDERFLOW_MAGIC(ah)
#endif

struct mem_stats mem_stats;

INIT_LIST_OF(struct alloc_header, memory_list);

#ifdef LOG_MEMORY_ALLOC
static void
dump_short_info(struct alloc_header *ah, const unsigned char *file, int line,
		const unsigned char *type)
{
	fprintf(stderr, "%p", PTR_AH2BASE(ah)), fflush(stderr);
	fprintf(stderr, ":%d", ah->size), fflush(stderr);
	if (type && *type) fprintf(stderr, " %s", type), fflush(stderr);
	fprintf(stderr, " @ %s:%d ", file, line), fflush(stderr);
	if (strcmp(file, ah->file) || line != ah->line)
		fprintf(stderr, "< %s:%d", ah->file, ah->line), fflush(stderr);
	if (ah->comment) fprintf(stderr, " [%s]", ah->comment), fflush(stderr);
	fprintf(stderr, "\n"), fflush(stderr);
}
#else
#define dump_short_info(a, b, c, d)
#endif

static void
dump_info(struct alloc_header *ah, const unsigned char *info,
	  const unsigned char *file, int line, const unsigned char *type)
{
	fprintf(stderr, "%p", PTR_AH2BASE(ah)); fflush(stderr);
	/* In some extreme cases, we may core here, as 'ah' can no longer point
	 * to valid memory area (esp. when used in debug_mem_free()). */
	fprintf(stderr, ":%d", ah->size);

	if (type && *type) fprintf(stderr, " \033[1m%s\033[0m", type);

	if (info && *info) fprintf(stderr, " %s", info);

	fprintf(stderr, " @ ");

	/* Following is commented out, as we want to print this out even when
	 * there're lions in ah, pointing us to evil places in memory, leading
	 * to segfaults and stuff like that. --pasky */
	/* if (file && (strcmp(file, ah->file) || line != ah->line)) */
	if (file) fprintf(stderr, "%s:%d, ", file, line), fflush(stderr);

	fprintf(stderr, "alloc'd at %s:%d", ah->file, ah->line);

	if (ah->comment) fprintf(stderr, " [%s]", ah->comment);

	fprintf(stderr, "\n");
}

#ifdef CHECK_AH_SANITY
static inline int
bad_ah_sanity(struct alloc_header *ah, const unsigned char *info,
	      const unsigned char *file, int line)
{
	if (!ah) return 1;
	if (ah->magic != AH_SANITY_MAGIC) {
		dump_info(ah, info, file, line, "bad alloc_header block");
		return 1;
	}

	return 0;
}
#endif /* CHECK_AH_SANITY */

#ifdef CHECK_XFLOWS
static inline int
bad_xflow_magic(struct alloc_header *ah, const unsigned char *info,
		const unsigned char *file, int line)
{
	if (!ah) return 1;

	if (*PTR_OVERFLOW_MAGIC(ah) != XFLOW_MAGIC) {
		dump_info(ah, info, file, line, "overflow detected");
		return 1;
	}

	if (*PTR_UNDERFLOW_MAGIC(ah) != XFLOW_MAGIC) {
		dump_info(ah, info, file, line, "underflow detected");
		return 1;
	}

	return 0;
}
#endif /* CHECK_XFLOWS */


void
check_memory_leaks(void)
{
	struct alloc_header *ah;

	if (!mem_stats.amount) {
		/* No leaks - escape now. */
		if (mem_stats.true_amount) /* debug memory leak ? */
			fprintf(stderr, "\n\033[1mDebug memory leak by %ld bytes\033[0m\n",
				mem_stats.true_amount);

		return;
	}

	fprintf(stderr, "\n\033[1mMemory leak by %ld bytes\033[0m\n",
		mem_stats.amount);

	fprintf(stderr, "List of blocks:\n");
	foreach (ah, memory_list) {
#ifdef CHECK_AH_SANITY
		if (bad_ah_sanity(ah, "Skipped", NULL, 0)) continue;
#endif
#ifdef CHECK_XFLOWS
		if (bad_xflow_magic(ah, NULL, NULL, 0)) continue;
#endif
		dump_info(ah, NULL, NULL, 0, NULL);
	}

	force_dump();
}

static int alloc_try = 0;

static int
patience(const unsigned char *file, int line, const unsigned char *of)
{
	errfile = file;
	errline = line;

	++alloc_try;
	if (alloc_try < ALLOC_MAXTRIES) {
		elinks_error("Out of memory (%s returned NULL): retry #%d/%d, "
			"I still exercise my patience and retry tirelessly.",
			of, alloc_try, ALLOC_MAXTRIES);
		sleep(ALLOC_DELAY);
		return alloc_try;
	}

#ifdef CRASH_IF_ALLOC_MAXTRIES
	elinks_internal("Out of memory (%s returned NULL) after %d tries, "
			"I give up. See ya on the other side.",
			of, alloc_try);
#else
	elinks_error("Out of memory (%s returned NULL) after %d tries, "
		     "I give up and try to continue. Pray for me, please.",
		     of, alloc_try);
#endif
	alloc_try = 0;
	return 0;
}

void *
debug_mem_alloc(const unsigned char *file, int line, size_t size)
{
	struct alloc_header *ah;
	size_t true_size;

	if (!size) return NULL;

	true_size = SIZE_BASE2AH(size);
	do {
#ifdef CONFIG_GC
		ah = GC_MALLOC(true_size);
#else
		ah = malloc(true_size);
#endif
		if (ah) break;
	} while (patience(file, line, "malloc"));
	if (!ah) return NULL;

#ifdef FILL_ON_ALLOC
	memset(ah, FILL_ON_ALLOC_VALUE, SIZE_BASE2AH(size));
#endif

	mem_stats.true_amount += true_size;
	mem_stats.amount += size;

	ah->size = size;
#ifdef CHECK_AH_SANITY
	ah->magic = AH_SANITY_MAGIC;
#endif
	ah->file = file;
	ah->line = line;
	ah->comment = NULL;
#ifdef CHECK_XFLOWS
	SET_XFLOW_MAGIC(ah);
#endif

	add_to_list(memory_list, ah);

	dump_short_info(ah, file, line, "malloc");

	return PTR_AH2BASE(ah);
}

void *
debug_mem_calloc(const unsigned char *file, int line, size_t eltcount, size_t eltsize)
{
	struct alloc_header *ah;
	size_t size = eltcount * eltsize;
	size_t true_size;

	if (!size) return NULL;

	/* FIXME: Unfortunately, we can't pass eltsize through to calloc()
	 * itself, because we add bloat like alloc_header to it, which is
	 * difficult to be measured in eltsize. Maybe we should round it up to
	 * next eltsize multiple, but would it be worth the possibly wasted
	 * space? Someone should make some benchmarks. If you still read this
	 * comment, it means YOU should help us and do the benchmarks! :)
	 * Thanks a lot. --pasky */

	true_size = SIZE_BASE2AH(size);
	do {
#ifdef CONFIG_GC
		ah = GC_MALLOC(true_size);
#else
		ah = calloc(1, true_size);
#endif
		if (ah) break;
	} while (patience(file, line, "calloc"));
	if (!ah) return NULL;

	/* No, we do NOT want to fill this with FILL_ON_ALLOC_VALUE ;)). */

	mem_stats.true_amount += true_size;
	mem_stats.amount += size;

	ah->size = size;
#ifdef CHECK_AH_SANITY
	ah->magic = AH_SANITY_MAGIC;
#endif
	ah->file = file;
	ah->line = line;
	ah->comment = NULL;
#ifdef CHECK_XFLOWS
	SET_XFLOW_MAGIC(ah);
#endif

	add_to_list(memory_list, ah);

	dump_short_info(ah, file, line, "calloc");

	return PTR_AH2BASE(ah);
}

void
debug_mem_free(const unsigned char *file, int line, void *ptr)
{
	struct alloc_header *ah;
	int ok = 1;

	if (!ptr) {
		errfile = file;
		errline = line;
		elinks_internal("mem_free(NULL)");
		return;
	}

#ifdef CHECK_INVALID_FREE
	ok = 0;
	foreach (ah, memory_list) {
		if (ah == PTR_BASE2AH(ptr)) {
			ok = 1;
			break;
		}
	}
#endif

	ah = PTR_BASE2AH(ptr);

	if (!ok) {
		/* This will get optimized out when not CHECK_INVALID_FREE. */
		dump_info(ah, "free()", file, line, "invalid address");
		return;
	}

#ifdef CHECK_DOUBLE_FREE
	if (ah->magic == AH_FREE_MAGIC) {
		dump_info(ah, "free()", file, line, "double free");
		/* If we already survived it chances are we could get over it.
		 * So let's not be overly tragic immediatelly. Just make sure
		 * the developer saw it. */
		sleep(1);
	}
#endif

#ifdef CHECK_AH_SANITY
	if (bad_ah_sanity(ah, "free()", file, line))
		force_dump();
#endif
#ifdef CHECK_XFLOWS
	if (bad_xflow_magic(ah, "free()", file, line))
		force_dump();
#endif

	dump_short_info(ah, file, line, "free");

	if (ah->comment) {
		mem_stats.true_amount -= strlen(ah->comment) + 1;
#ifdef CONFIG_GC
		ah->comment = NULL;
#else
		free(ah->comment);
#endif
	}

	del_from_list(ah);

	mem_stats.true_amount -= SIZE_BASE2AH(ah->size);
	mem_stats.amount -= ah->size;

#ifdef FILL_ON_FREE
	memset(ah, FILL_ON_FREE_VALUE, SIZE_BASE2AH(ah->size));
#endif
#ifdef CHECK_DOUBLE_FREE
	ah->magic = AH_FREE_MAGIC;
#endif

#ifdef CONFIG_GC
	ah = NULL;
#else
	free(ah);
#endif
}

void *
debug_mem_realloc(const unsigned char *file, int line, void *ptr, size_t size)
{
	struct alloc_header *ah, *ah2;
	size_t true_size;

	if (!ptr) return debug_mem_alloc(file, line, size);

	/* Frees memory if size is zero. */
	if (!size) {
		debug_mem_free(file, line, ptr);
		return NULL;
	}

	ah = PTR_BASE2AH(ptr);

#ifdef CHECK_AH_SANITY
	if (bad_ah_sanity(ah, "realloc()", file, line)) force_dump();
#endif
#ifdef CHECK_XFLOWS
	if (bad_xflow_magic(ah, "realloc()", file, line)) force_dump();
#endif

	/* We compare oldsize to new size, and if equal we just return ptr
	 * and change nothing, this conforms to usual realloc() behavior. */
	if (ah->size == size) {
#ifdef CHECK_USELESS_REALLOC
		fprintf(stderr, "[%s:%d] mem_realloc() oldsize = newsize = %zu\n", file, line, size);
#endif
		return (void *) ptr;
	}

	true_size = SIZE_BASE2AH(size);
	do {
#ifdef CONFIG_GC
		ah2 = GC_REALLOC(ah, true_size);
#else
		ah2 = realloc(ah, true_size);
#endif
		if (ah2) {
			ah = ah2;
			break;
		}
	} while (patience(file, line, "realloc"));
	if (!ah2) return NULL;

	mem_stats.true_amount += true_size - SIZE_BASE2AH(ah->size);
	mem_stats.amount += size - ah->size;

#ifdef FILL_ON_REALLOC
	if (size > ah->size)
		memset((char *) PTR_AH2BASE(ah) + ah->size,
		       FILL_ON_REALLOC_VALUE, size - ah->size);
#endif

	ah->size = size;
#ifdef CHECK_AH_SANITY
	ah->magic = AH_SANITY_MAGIC;
#endif
	ah->file = file;
	ah->line = line;
#ifdef CHECK_XFLOWS
	SET_XFLOW_MAGIC(ah);
#endif

	ah->prev->next = ah;
	ah->next->prev = ah;

	dump_short_info(ah, file, line, "realloc");

	return PTR_AH2BASE(ah);
}

void
set_mem_comment(void *ptr, const unsigned char *str, int len)
{
	struct alloc_header *ah;

	ah = PTR_BASE2AH(ptr);

	if (ah->comment) {
		mem_stats.true_amount -= strlen(ah->comment) + 1;
#ifdef CONFIG_GC
		ah->comment = NULL;
#else
		free(ah->comment);
#endif
	}
#ifdef CONFIG_GC
	ah->comment = GC_MALLOC(len + 1);
#else
	ah->comment = malloc(len + 1);
#endif
	if (ah->comment) {
		memcpy(ah->comment, str, len);
		ah->comment[len] = 0;
		mem_stats.true_amount += len + 1;
	}
}

#endif /* DEBUG_MEMLEAK */
