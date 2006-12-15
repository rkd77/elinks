/* Bzip2 encoding (ENCODING_BZIP2) backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_BZLIB_H
#include <bzlib.h> /* Everything needs this after stdio.h */
#endif

#include "elinks.h"

#include "encoding/bzip2.h"
#include "encoding/encoding.h"
#include "util/memory.h"

struct bz2_enc_data {
	FILE *file;
	BZFILE *bzfile;
	int last_read; /* If err after last bzRead() was BZ_STREAM_END.. */
};

/* TODO: When it'll be official, use bzdopen() from Yoshioka Tsuneo. --pasky */

static int
bzip2_open(struct stream_encoded *stream, int fd)
{
	struct bz2_enc_data *data = mem_alloc(sizeof(*data));
	int err;

	if (!data) {
		return -1;
	}
	data->last_read = 0;

	data->file = fdopen(fd, "rb");

	data->bzfile = BZ2_bzReadOpen(&err, data->file, 0, 0, NULL, 0);
	if (!data->bzfile) {
		mem_free(data);
		return -1;
	}

	stream->data = data;

	return 0;
}

static int
bzip2_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct bz2_enc_data *data = (struct bz2_enc_data *) stream->data;
	int err = 0;

	if (data->last_read)
		return 0;

	clearerr(data->file);
	len = BZ2_bzRead(&err, data->bzfile, buf, len);

	if (err == BZ_STREAM_END)
		data->last_read = 1;
	else if (err)
		return -1;

	return len;
}

static unsigned char *
bzip2_decode(struct stream_encoded *stream, unsigned char *data, int len,
	     int *new_len)
{
	*new_len = len;
	return data;
}

#ifdef CONFIG_SMALL
#define BZIP2_SMALL 1
#else
#define BZIP2_SMALL 0
#endif

static unsigned char *
bzip2_decode_buffer(unsigned char *data, int len, int *new_len)
{
	bz_stream stream;
	unsigned char *buffer = NULL;
	int error;

	memset(&stream, 0, sizeof(bz_stream));
	stream.next_in = data;
	stream.avail_in = len;

	if (BZ2_bzDecompressInit(&stream, 0, BZIP2_SMALL) != BZ_OK)
		return NULL;

	do {
		unsigned char *new_buffer;
		size_t size = stream.total_out_lo32 + MAX_STR_LEN;

		/* FIXME: support for 64 bit.  real size is
		 *
		 * 	(total_in_hi32 << * 32) + total_in_lo32
		 *
		 * --jonas */
		assertm(!stream.total_out_hi32, "64 bzip2 decoding not supported");

		new_buffer = mem_realloc(buffer, size);
		if (!new_buffer) {
			error = BZ_MEM_ERROR;
			break;
		}

		buffer		 = new_buffer;
		stream.next_out  = buffer + stream.total_out_lo32;
		stream.avail_out = MAX_STR_LEN;

		error = BZ2_bzDecompress(&stream);
		if (error == BZ_STREAM_END) {
			*new_len = stream.total_out_lo32;
			error = BZ_OK;
			break;
		}

		/* Apparently BZ_STREAM_END is not forced when the end of input
		 * is reached. At least lindi- reported that it caused a
		 * reproducable infinite loop. Maybe it has to do with decoding
		 * an incomplete file. */
	} while (error == BZ_OK && stream.avail_in > 0);

	BZ2_bzDecompressEnd(&stream);

	if (error != BZ_OK) {
		if (buffer) mem_free(buffer);
		*new_len = 0;
		return NULL;
	}

	return buffer;
}

static void
bzip2_close(struct stream_encoded *stream)
{
	struct bz2_enc_data *data = (struct bz2_enc_data *) stream->data;
	int err;

	BZ2_bzReadClose(&err, data->bzfile);
	fclose(data->file);
	mem_free(data);
}

static unsigned char *bzip2_extensions[] = { ".bz2", ".tbz", NULL };

struct decoding_backend bzip2_decoding_backend = {
	"bzip2",
	bzip2_extensions,
	bzip2_open,
	bzip2_read,
	bzip2_decode,
	bzip2_decode_buffer,
	bzip2_close,
};
