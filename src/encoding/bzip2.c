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

#define ELINKS_BZ_BUFFER_LENGTH BZ_MAX_UNUSED

struct bz2_enc_data {
	unsigned char buf[ELINKS_BZ_BUFFER_LENGTH];
	bz_stream fbz_stream;
	int fdread;
	int last_read; /* If err after last bzDecompress was BZ_STREAM_END.. */
};

static int
bzip2_open(struct stream_encoded *stream, int fd)
{
	struct bz2_enc_data *data = mem_alloc(sizeof(*data));
	int err;

	stream->data = 0;
	if (!data) {
		return -1;
	}
	memset(data, 0, sizeof(struct bz2_enc_data) - ELINKS_BZ_BUFFER_LENGTH);

	data->last_read = 0;
	data->fdread = fd;
	
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
	
	return len - data->fbz_stream.avail_out;
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

	if (data) {
		BZ2_bzDecompressEnd(&data->fbz_stream);
		close(data->fdread);
		mem_free(data);
		stream->data = 0;
	}
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
