#ifndef EL__UTIL_MEMCOUNT_H
#define EL__UTIL_MEMCOUNT_H

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

#ifdef __cplusplus
}
#endif

#endif

