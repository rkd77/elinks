#ifndef EL__ENCODING_GZIP_H
#define EL__ENCODING_GZIP_H

#include "encoding/encoding.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_GZIP
extern const struct decoding_backend gzip_decoding_backend;
#else
#define gzip_decoding_backend dummy_decoding_backend
#endif

#ifdef __cplusplus
}
#endif

#endif
