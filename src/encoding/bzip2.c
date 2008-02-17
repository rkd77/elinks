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
#include <errno.h>

#include "elinks.h"

#include "encoding/bzip2.h"
#include "encoding/encoding.h"
#include "util/memory.h"

/* How many bytes of compressed data to read before decompressing.
 * This is currently defined as BZ_MAX_UNUSED to make the behaviour
 * similar to BZ2_bzRead; but other values would work too.  */
#define ELINKS_BZ_BUFFER_LENGTH BZ_MAX_UNUSED

struct bz2_enc_data {
	bz_stream fbz_stream;

	/* The file descriptor from which we read.  */
	int fdread;

	/* Initially 0; set to 1 when BZ2_bzDecompress indicates
	 * BZ_STREAM_END, which means it has found the bzip2-specific
	 * end-of-stream marker and all data has been decompressed.
	 * Then we neither read from the file nor call BZ2_bzDecompress
	 * any more.  */
	int last_read;

	/* A buffer for data that has been read from the file but not
	 * yet decompressed.  fbz_stream.next_in and fbz_stream.avail_in
	 * refer to this buffer.  */
	unsigned char buf[ELINKS_BZ_BUFFER_LENGTH];
};

static int
bzip2_open(struct stream_encoded *stream, int fd)
{
	/* A zero-initialized bz_stream.  The compiler ensures that all
	 * pointer members in it are null.  (Can't do this with memset
	 * because C99 does not require all-bits-zero to be a null
	 * pointer.)  */
	static const bz_stream null_bz_stream = {0};

	struct bz2_enc_data *data = mem_alloc(sizeof(*data));
	int err;

	stream->data = NULL;
	if (!data) {
		return -1;
	}

	/* Initialize all members of *data, except data->buf[], which
	 * will be initialized on demand by bzip2_read.  */
	copy_struct(&data->fbz_stream, &null_bz_stream);
	data->fdread = fd;
	data->last_read = 0;

	err = BZ2_bzDecompressInit(&data->fbz_stream, 0, 0);
	if (err != BZ_OK) {
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

	if (!data) return -1;

	assert(len > 0);

	if (data->last_read) return 0;

	data->fbz_stream.avail_out = len;
	data->fbz_stream.next_out = buf;

	do {
		if (data->fbz_stream.avail_in == 0) {
			int l = safe_read(data->fdread, data->buf,
			                  ELINKS_BZ_BUFFER_LENGTH);

			if (l == -1) {
				if (errno == EAGAIN)
					break;
				else
					return -1; /* I/O error */
			} else if (l == 0) {
				/* EOF. It is error: we wait for more bytes */
				return -1;
			}

			data->fbz_stream.next_in = data->buf;
			data->fbz_stream.avail_in = l;
		}

		err = BZ2_bzDecompress(&data->fbz_stream);
		if (err == BZ_STREAM_END) {
			data->last_read = 1;
			break;
		} else if (err != BZ_OK) {
			return -1;
		}
	} while (data->fbz_stream.avail_out > 0);

	assert(len - data->fbz_stream.avail_out == data->fbz_stream.next_out - (char *) buf);
	return len - data->fbz_stream.avail_out;
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

	*new_len = 0;	  /* default, left there if an error occurs */

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
			error = BZ_OK;
			break;
		}

		/* Apparently BZ_STREAM_END is not forced when the end of input
		 * is reached. At least lindi- reported that it caused a
		 * reproducable infinite loop. Maybe it has to do with decoding
		 * an incomplete file. */
	} while (error == BZ_OK && stream.avail_in > 0);

	BZ2_bzDecompressEnd(&stream);

	if (error == BZ_OK) {
		*new_len = stream.total_out_lo32;
		return buffer;
	} else {
		if (buffer) mem_free(buffer);
		return NULL;
	}
}

static void
bzip2_close(struct stream_encoded *stream)
{
	struct bz2_enc_data *data = (struct bz2_enc_data *) stream->data;

	if (data) {
		BZ2_bzDecompressEnd(&data->fbz_stream);
		close(data->fdread);
		mem_free(data);
		stream->data = 0;
	}
}

static const unsigned char *const bzip2_extensions[] = { ".bz2", ".tbz", NULL };

const struct decoding_backend bzip2_decoding_backend = {
	"bzip2",
	bzip2_extensions,
	bzip2_open,
	bzip2_read,
	bzip2_decode_buffer,
	bzip2_close,
};
