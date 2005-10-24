
#ifndef EL__ENCODING_GZIP_H
#define EL__ENCODING_GZIP_H

#include "encoding/encoding.h"

#ifdef CONFIG_GZIP
extern struct decoding_backend gzip_decoding_backend;
#else
#define gzip_decoding_backend dummy_decoding_backend
#endif

#endif
