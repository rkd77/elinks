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
