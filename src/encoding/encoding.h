#ifndef EL__ENCODING_ENCODING_H
#define EL__ENCODING_ENCODING_H

#include "network/state.h"
#include "util/string.h"

enum stream_encoding {
	ENCODING_NONE = 0,
	ENCODING_GZIP,
	ENCODING_BZIP2,
	ENCODING_LZMA,
	ENCODING_DEFLATE,

	/* Max. number of known encoding including ENCODING_NONE. */
	ENCODINGS_KNOWN,
};

/** Special values returned by read_encoded() and
 * decoding_backend.read.  Positive numbers cannot be used in
 * this enum because they mean byte counts as return values.
 * Zero could be used but currently is not used.
 * Do not rely on the order of values here.  */
enum read_encoded_result {
	/** An error occurred and the code is in @c errno.  */
	READENC_ERRNO		= -1,

	/** Saw an end-of-file mark in the compressed data.  */
	READENC_STREAM_END	= -2,

	/** The data ended before the decompressor expected.  */
	READENC_UNEXPECTED_EOF	= -3,

	/** Cannot decompress anything yet: please provide more data.  */
	READENC_EAGAIN		= -4,

	/** The input data is malformed: for example, checksums don't
	 * match, or a header is missing.  */
	READENC_DATA_ERROR	= -5,

	/** Out of memory */
	READENC_MEM_ERROR	= -6,

	/** An internal error occurred.  */
	READENC_INTERNAL	= -7
};

struct stream_encoded {
	enum stream_encoding encoding;
	void *data;
};

struct decoding_backend {
	const unsigned char *name;
	const unsigned char *const *extensions;

	int (*open)(struct stream_encoded *stream, int fd);

	/*! @return A positive number means that many bytes were
	 * written to the @a data array.  Otherwise, the value is
	 * enum read_encoded_result.  */
	int (*read)(struct stream_encoded *stream, unsigned char *data, int len);

	unsigned char *(*decode_buffer)(unsigned char *data, int len, int *new_len);

	void (*close)(struct stream_encoded *stream);
};

struct stream_encoded *open_encoded(int, enum stream_encoding);
int read_encoded(struct stream_encoded *, unsigned char *, int);
unsigned char *decode_encoded_buffer(enum stream_encoding encoding, unsigned char *data, int len, int *new_len);
void close_encoded(struct stream_encoded *);

const unsigned char *const *listext_encoded(enum stream_encoding);
enum stream_encoding guess_encoding(unsigned char *filename);
const unsigned char *get_encoding_name(enum stream_encoding encoding);

/* Read from open @stream into the @page string */
struct connection_state
read_file(struct stream_encoded *stream, int readsize, struct string *page);

/* Reads the file with the given @filename into the string @source. */
struct connection_state read_encoded_file(struct string *filename, struct string *source);

#endif
