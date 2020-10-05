#ifndef EL__ENCODING_ZSTD_H
#define EL__ENCODING_ZSTD_H

#include "encoding/encoding.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_ZSTD
extern const struct decoding_backend zstd_decoding_backend;
#else
#define zstd_decoding_backend dummy_decoding_backend
#endif

#ifdef __cplusplus
}
#endif

#endif
