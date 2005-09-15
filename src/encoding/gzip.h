/* $Id: gzip.h,v 1.1 2004/05/28 11:55:26 jonas Exp $ */

#ifndef EL__ENCODING_GZIP_H
#define EL__ENCODING_GZIP_H

#include "encoding/encoding.h"

#ifdef CONFIG_GZIP
extern struct decoding_backend gzip_decoding_backend;
#else
#define gzip_decoding_backend dummy_decoding_backend
#endif

#endif
