#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include <string.h>
#include <malloc.h>
#include <pthread.h>

#include "util/memcount.h"
#include "util/memory.h"
#include "util/string.h"

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
	free(ptr);
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
	free(ptr);
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

#ifdef CONFIG_LIBCURL

static std::map<void *, uint64_t> el_curl_allocs;
static uint64_t el_curl_total_allocs;
static uint64_t el_curl_size;

/* call custom malloc() */
void *
el_curl_malloc(
    size_t              /* in */ size)          /* allocation size */
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&mutex);

	void *res = mem_alloc(size);

	if (res) {
		el_curl_allocs[res] = size;
		el_curl_total_allocs++;
		el_curl_size += size;
	}
	pthread_mutex_unlock(&mutex);

	return res;
}

/* call custom strdup() */
char *
el_curl_strdup(const char *str)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&mutex);

	if (!str) {
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	size_t size = strlen(str) + 1;
	char *ret = stracpy(str);

	if (ret) {
		el_curl_allocs[(void *)ret] = size;
		el_curl_total_allocs++;
		el_curl_size += size;
	}
	pthread_mutex_unlock(&mutex);

	return ret;
}

/* call custom calloc() */
void *
el_curl_calloc(
    size_t              /* in */ nelm,        /* allocation size */
    size_t              /* in */ elsize)     /* allocation size */
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&mutex);

	uint64_t alloc_size = nelm * elsize;
	void *res = mem_calloc(nelm, elsize);

	if (res) {
		el_curl_allocs[res] = alloc_size;
		el_curl_total_allocs++;
		el_curl_size += alloc_size;
	}
	pthread_mutex_unlock(&mutex);

	return res;
}

/* call custom realloc() */
void *
el_curl_realloc(
    void                /* in */ *p,          /* existing buffer to be re-allocated */
    size_t              /* in */ n)          /* re-allocation size */
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	if (!p) {
		return el_curl_malloc(n);
	}
	pthread_mutex_lock(&mutex);
	auto el = el_curl_allocs.find(p);
	size_t size = 0;
	bool todelete = false;

	if (el == el_curl_allocs.end()) {
		fprintf(stderr, "curl realloc %p not found\n", p);
	} else {
		size = el->second;
		todelete = true;
	}
	void *ret = mem_realloc(p, n);
	if (todelete) {
		el_curl_allocs.erase(el);
	}

	if (ret) {
		el_curl_allocs[ret] = n;
		el_curl_total_allocs++;
		el_curl_size += n - size;
	}
	pthread_mutex_unlock(&mutex);

	return ret;
}

/* call custom free() */
void
el_curl_free(
    void                /* in */ *p)         /* existing buffer to be freed */
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	if (!p) {
		return;
	}
	pthread_mutex_lock(&mutex);

	auto el = el_curl_allocs.find(p);

	if (el == el_curl_allocs.end()) {
		fprintf(stderr, "curl free %p not found\n", p);
		pthread_mutex_unlock(&mutex);
		return;
	}
	el_curl_size -= el->second;
	el_curl_allocs.erase(el);
	mem_free(p);
	pthread_mutex_unlock(&mutex);
}

uint64_t
get_curl_total_allocs(void)
{
	return el_curl_total_allocs;
}

uint64_t
get_curl_size(void)
{
	return el_curl_size;
}

uint64_t
get_curl_active(void)
{
	return el_curl_allocs.size();
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
	if (!p) {
		return el_sixel_malloc(n);
	}
	auto el = el_sixel_allocs.find(p);
	size_t size = 0;
	bool todelete = false;

	if (el == el_sixel_allocs.end()) {
		fprintf(stderr, "sixel %p not found\n", p);
	} else {
		size = el->second;
		todelete = true;
	}
	void *ret = realloc(p, n);

	if (todelete) {
		el_sixel_allocs.erase(el);
	}
	if (ret) {
		el_sixel_allocs[ret] = n;
		el_sixel_total_allocs++;
		el_sixel_size += n - size;
	}

	return ret;
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
	free(p);
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

#ifdef CONFIG_MUJS
static std::map<void *, uint64_t> el_mujs_allocs;
static uint64_t el_mujs_total_allocs;
static uint64_t el_mujs_size;

void *
el_mujs_alloc(void *memctx, void *ptr, int size)
{
	if (size == 0) {
		if (!ptr) {
			return NULL;
		}
		auto el = el_mujs_allocs.find(ptr);

		if (el == el_mujs_allocs.end()) {
			fprintf(stderr, "mujs free %p not found\n", ptr);
			return NULL;
		}
		free(ptr);
		el_mujs_allocs.erase(el);
		return NULL;
	}
	size_t oldsize = 0;
	if (ptr) {
		auto el = el_mujs_allocs.find(ptr);

		if (el == el_mujs_allocs.end()) {
			fprintf(stderr, "mujs realloc %p not found\n", ptr);
		} else {
			oldsize = el->second;
			el_mujs_allocs.erase(el);
		}
	}
	void *ret = realloc(ptr, (size_t)size);

	if (ret) {
		el_mujs_allocs[ret] = (uint64_t)size;
		el_mujs_total_allocs++;
		el_mujs_size += size - oldsize;
	}

	return ret;
}

uint64_t
get_mujs_total_allocs(void)
{
	return el_mujs_total_allocs;
}

uint64_t
get_mujs_size(void)
{
	return el_mujs_size;
}

uint64_t
get_mujs_active(void)
{
	return el_mujs_allocs.size();
}
#endif

#ifdef CONFIG_QUICKJS
static std::map<void *, uint64_t> el_quickjs_allocs;
static uint64_t el_quickjs_total_allocs;
static uint64_t el_quickjs_size;

static void *
el_quickjs_malloc(JSMallocState *s, size_t size)
{
	void *res = malloc(size);

	if (res) {
		el_quickjs_allocs[res] = size;
		el_quickjs_total_allocs++;
		el_quickjs_size += size;
	}

	return res;
}

static void
el_quickjs_free(JSMallocState *s, void *ptr)
{
	if (!ptr) {
		return;
	}

	auto el = el_quickjs_allocs.find(ptr);

	if (el == el_quickjs_allocs.end()) {
		fprintf(stderr, "quickjs free %p not found\n", ptr);
		return;
	}
	el_quickjs_size -= el->second;
	el_quickjs_allocs.erase(el);
	free(ptr);
}

static void *
el_quickjs_realloc(JSMallocState *s, void *ptr, size_t n)
{
	if (!ptr) {
		return el_quickjs_malloc(s, n);
	}
	auto el = el_quickjs_allocs.find(ptr);
	size_t size = 0;

	if (el == el_quickjs_allocs.end()) {
		fprintf(stderr, "quickjs realloc %p not found\n", ptr);
	} else {
		size = el->second;
		el_quickjs_allocs.erase(el);
	}
	void *ret = realloc(ptr, n);

	if (ret) {
		el_quickjs_allocs[ret] = n;
		el_quickjs_total_allocs++;
		el_quickjs_size += n - size;
	}

	return ret;
}

static size_t
el_quickjs_malloc_usable_size(const void *ptr)
{
#if defined(__APPLE__)
	return malloc_size(ptr);
#elif defined(_WIN32)
	return _msize(ptr);
#elif defined(EMSCRIPTEN)
	return 0;
#elif defined(__linux__)
	return malloc_usable_size((void *)ptr);
#else
	/* change this to `return 0;` if compilation fails */
	return malloc_usable_size((void *)ptr);
#endif
}

const JSMallocFunctions el_quickjs_mf = {
	el_quickjs_malloc,
	el_quickjs_free,
	el_quickjs_realloc,
	el_quickjs_malloc_usable_size
};

uint64_t
get_quickjs_total_allocs(void)
{
	return el_quickjs_total_allocs;
}

uint64_t
get_quickjs_size(void)
{
	return el_quickjs_size;
}

uint64_t
get_quickjs_active(void)
{
	return el_quickjs_allocs.size();
}
#endif

#ifdef CONFIG_ZSTD
static std::map<void *, uint64_t> el_zstd_allocs;
static uint64_t el_zstd_total_allocs;
static uint64_t el_zstd_size;

static void *
el_zstd_malloc(void *s, size_t size)
{
	void *res = malloc(size);

	if (res) {
		el_zstd_allocs[res] = size;
		el_zstd_total_allocs++;
		el_zstd_size += size;
	}

	return res;
}

static void
el_zstd_free(void *s, void *ptr)
{
	if (!ptr) {
		return;
	}

	auto el = el_zstd_allocs.find(ptr);

	if (el == el_zstd_allocs.end()) {
		fprintf(stderr, "zstd free %p not found\n", ptr);
		return;
	}
	el_zstd_size -= el->second;
	el_zstd_allocs.erase(el);
	free(ptr);
}

ZSTD_customMem el_zstd_mf = {
	el_zstd_malloc,
	el_zstd_free,
	NULL
};

uint64_t
get_zstd_total_allocs(void)
{
	return el_zstd_total_allocs;
}

uint64_t
get_zstd_size(void)
{
	return el_zstd_size;
}

uint64_t
get_zstd_active(void)
{
	return el_zstd_allocs.size();
}
#endif
