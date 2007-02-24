#ifndef EL__ENCODING_BZIP2_H
#define EL__ENCODING_BZIP2_H

#include "encoding/encoding.h"

#ifdef CONFIG_BZIP2
extern const struct decoding_backend bzip2_decoding_backend;
#else
#define bzip2_decoding_backend dummy_decoding_backend
#endif

#endif
