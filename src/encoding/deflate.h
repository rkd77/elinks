#ifndef EL__ENCODING_DEFLATE_H
#define EL__ENCODING_DEFLATE_H

#include "encoding/encoding.h"

#ifdef CONFIG_GZIP
extern const struct decoding_backend deflate_decoding_backend;
extern const struct decoding_backend gzip_decoding_backend;
#else
#define deflate_decoding_backend dummy_decoding_backend
#define gzip_decoding_backend dummy_decoding_backend
#endif

#endif
