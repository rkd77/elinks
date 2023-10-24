#ifndef EL__UTIL_MEMCOUNT_H
#define EL__UTIL_MEMCOUNT_H

#ifdef CONFIG_LIBSIXEL
#include <sixel.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_BROTLI
void *el_brotli_alloc(void *opaque, size_t size);
void el_brotli_free(void  *opaque, void *ptr);
uint64_t get_brotli_total_allocs(void);
uint64_t get_brotli_size(void);
uint64_t get_brotli_active(void);
#endif

#ifdef CONFIG_GZIP
void *el_gzip_alloc(void *opaque, unsigned int items, unsigned int size);
void el_gzip_free(void  *opaque, void *ptr);
uint64_t get_gzip_total_allocs(void);
uint64_t get_gzip_size(void);
uint64_t get_gzip_active(void);
#endif

#ifdef CONFIG_LIBCURL
void *el_curl_malloc(size_t size);
void *el_curl_calloc(size_t nelm, size_t elsize);
void *el_curl_realloc(void *p, size_t n);
char *el_curl_strdup(const char *str);
void el_curl_free(void *p);
uint64_t get_curl_total_allocs(void);
uint64_t get_curl_size(void);
uint64_t get_curl_active(void);
#endif

#ifdef CONFIG_LIBSIXEL
void *el_sixel_malloc(size_t size);
void *el_sixel_calloc(size_t nelm, size_t elsize);
void *el_sixel_realloc(void *p, size_t n);
void el_sixel_free(void *p);
uint64_t get_sixel_total_allocs(void);
uint64_t get_sixel_size(void);
uint64_t get_sixel_active(void);
#endif

#ifdef CONFIG_MUJS
void *el_mujs_alloc(void *memctx, void *ptr, int size);
uint64_t get_mujs_total_allocs(void);
uint64_t get_mujs_size(void);
uint64_t get_mujs_active(void);
#endif

#ifdef __cplusplus
}
#endif

#endif

