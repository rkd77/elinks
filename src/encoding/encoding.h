#ifndef EL__ENCODING_ENCODING_H
#define EL__ENCODING_ENCODING_H

#include "network/state.h"
#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif

enum stream_encoding {
	ENCODING_NONE = 0,
	ENCODING_GZIP,
	ENCODING_BZIP2,
	ENCODING_LZMA,
	ENCODING_BROTLI,
	ENCODING_ZSTD,

	/* Max. number of known encoding including ENCODING_NONE. */
	ENCODINGS_KNOWN,
};

typedef unsigned char stream_encoding_T;

struct stream_encoded {
	stream_encoding_T encoding;
	void *data;
};

struct decoding_backend {
	const char *name;
	const char *const *extensions;
	int (*eopen)(struct stream_encoded *stream, int fd);
	int (*eread)(struct stream_encoded *stream, char *data, int len);
	char *(*decode_buffer)(struct stream_encoded *stream, char *data, int len, int *new_len);
	void (*eclose)(struct stream_encoded *stream);
};

struct stream_encoded *open_encoded(int, stream_encoding_T);
int read_encoded(struct stream_encoded *, char *, int);
char *decode_encoded_buffer(struct stream_encoded *stream, stream_encoding_T encoding, char *data, int len, int *new_len);
void close_encoded(struct stream_encoded *);

const char *const *listext_encoded(stream_encoding_T);
stream_encoding_T guess_encoding(char *filename);
const char *get_encoding_name(stream_encoding_T encoding);

/* Read from open @stream into the @page string */
struct connection_state
read_file(struct stream_encoded *stream, int readsize, struct string *page);

/* Reads the file with the given @filename into the string @source. */
struct connection_state read_encoded_file(struct string *filename, struct string *source);

#ifdef __cplusplus
}
#endif

#endif
