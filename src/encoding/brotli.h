#ifndef EL__ENCODING_BROTLI_H
#define EL__ENCODING_BROTLI_H

#include "encoding/encoding.h"

#ifdef CONFIG_BROTLI
extern const struct decoding_backend brotli_decoding_backend;
#else
#define brotli_decoding_backend dummy_decoding_backend
#endif

#endif
