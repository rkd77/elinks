#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"
#include "util/memcount.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>

#ifdef CONFIG_BROTLI

static std::map<void *, uint64_t> el_brotli_allocs;
static uint64_t el_brotli_total_allocs;
static uint64_t el_brotli_size;

void *
el_brotli_alloc(void *opaque, size_t size)
{
	void *res = malloc(size);

	if (res) {
		el_brotli_allocs[res] = size;
		el_brotli_total_allocs++;
		el_brotli_size += size;
	}

	return res;
}

void
el_brotli_free(void *opaque, void *ptr)
{
	if (!ptr) {
		return;
	}

	auto el = el_brotli_allocs.find(ptr);

	if (el == el_brotli_allocs.end()) {
		fprintf(stderr, "brotli %p not found\n", ptr);
		return;
	}
	el_brotli_size -= el->second;
	el_brotli_allocs.erase(el);
}

uint64_t
get_brotli_total_allocs(void)
{
	return el_brotli_total_allocs;
}

uint64_t
get_brotli_size(void)
{
	return el_brotli_size;
}

uint64_t
get_brotli_active(void)
{
	return el_brotli_allocs.size();
}
#endif


#ifdef CONFIG_GZIP

static std::map<void *, uint64_t> el_gzip_allocs;
static uint64_t el_gzip_total_allocs;
static uint64_t el_gzip_size;

void *
el_gzip_alloc(void *opaque, unsigned int items, unsigned int size)
{
	uint64_t alloc_size = items * size;
	void *res = calloc(items, size);

	if (res) {
		el_gzip_allocs[res] = alloc_size;
		el_gzip_total_allocs++;
		el_gzip_size += alloc_size;
	}

	return res;
}

void
el_gzip_free(void  *opaque, void *ptr)
{
	auto el = el_gzip_allocs.find(ptr);

	if (el == el_gzip_allocs.end()) {
		fprintf(stderr, "gzip %p not found\n", ptr);
		return;
	}
	el_gzip_size -= el->second;
	el_gzip_allocs.erase(el);
}

uint64_t
get_gzip_total_allocs(void)
{
	return el_gzip_total_allocs;
}

uint64_t
get_gzip_size(void)
{
	return el_gzip_size;
}

uint64_t
get_gzip_active(void)
{
	return el_gzip_allocs.size();
}
#endif

#ifdef CONFIG_LIBSIXEL

static std::map<void *, uint64_t> el_sixel_allocs;
static uint64_t el_sixel_total_allocs;
static uint64_t el_sixel_size;

/* call custom malloc() */
void *
el_sixel_malloc(
    size_t              /* in */ size)          /* allocation size */
{
	void *res = malloc(size);

	if (res) {
		el_sixel_allocs[res] = size;
		el_sixel_total_allocs++;
		el_sixel_size += size;
	}

	return res;
}


/* call custom calloc() */
void *
el_sixel_calloc(
    size_t              /* in */ nelm,        /* allocation size */
    size_t              /* in */ elsize)     /* allocation size */
{
	uint64_t alloc_size = nelm * elsize;
	void *res = calloc(nelm, elsize);

	if (res) {
		el_sixel_allocs[res] = alloc_size;
		el_sixel_total_allocs++;
		el_sixel_size += alloc_size;
	}

	return res;
}

/* call custom realloc() */
void *
el_sixel_realloc(
    void                /* in */ *p,          /* existing buffer to be re-allocated */
    size_t              /* in */ n)          /* re-allocation size */
{
	el_sixel_free(p);
	return el_sixel_malloc(n);
}

/* call custom free() */
void
el_sixel_free(
    void                /* in */ *p)         /* existing buffer to be freed */
{
	if (!p) {
		return;
	}

	auto el = el_sixel_allocs.find(p);

	if (el == el_sixel_allocs.end()) {
		fprintf(stderr, "sixel %p not found\n", p);
		return;
	}
	el_sixel_size -= el->second;
	el_sixel_allocs.erase(el);
}

uint64_t
get_sixel_total_allocs(void)
{
	return el_sixel_total_allocs;
}

uint64_t
get_sixel_size(void)
{
	return el_sixel_size;
}

uint64_t
get_sixel_active(void)
{
	return el_sixel_allocs.size();
}
#endif
