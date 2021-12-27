#ifndef EL__ENCODING_BROTLI_H
#define EL__ENCODING_BROTLI_H

#include "encoding/encoding.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_BROTLI
extern const struct decoding_backend brotli_decoding_backend;
const char *get_brotli_version(void);

#else
#define brotli_decoding_backend dummy_decoding_backend
#endif

#ifdef __cplusplus
}
#endif

#endif
