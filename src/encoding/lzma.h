#ifndef EL__ENCODING_LZMA_H
#define EL__ENCODING_LZMA_H

#include "encoding/encoding.h"

#ifdef CONFIG_LZMA
extern const struct decoding_backend lzma_decoding_backend;
#else
#define lzma_decoding_backend dummy_decoding_backend
#endif

#endif
