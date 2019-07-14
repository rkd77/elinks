/* zstd encoding (ENCODING_ZSTD) backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZSTD_H
#include <zstd.h>
#endif
#include <errno.h>

#include "elinks.h"

#include "encoding/zstd.h"
#include "encoding/encoding.h"
#include "util/memory.h"

/* How many bytes of compressed data to read before decompressing.
 */
#define ELINKS_ZSTD_BUFFER_LENGTH 16384

struct zstd_enc_data {
	ZSTD_DCtx *zstd_stream;
	ZSTD_inBuffer input;
	ZSTD_outBuffer output;
	/* The file descriptor from which we read.  */
	int fdread;
	int last_read:1;

	/* A buffer for data that has been read from the file but not
	 * yet decompressed.  fbz_stream.next_in and fbz_stream.avail_in
	 * refer to this buffer.  */
	unsigned char buf[ELINKS_ZSTD_BUFFER_LENGTH];
};

static int
zstd_open(struct stream_encoded *stream, int fd)
{
	struct zstd_enc_data *data = mem_calloc(1, sizeof(*data));

	stream->data = NULL;
	if (!data) {
		return -1;
	}

	data->fdread = fd;
	data->zstd_stream = ZSTD_createDCtx();

	if (!data->zstd_stream) {
		mem_free(data);
		return -1;
	}

	stream->data = data;

	return 0;
}

static int
zstd_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct zstd_enc_data *data = (struct zstd_enc_data *) stream->data;
	int err = 0;

	if (!data) return -1;

	assert(len > 0);

	if (data->last_read) {
		return 0;
	}

	data->output.size = len;
	data->output.dst = buf;
	data->output.pos = 0;

	do {
		if (data->output.pos == data->output.size) {
			break;
		}
		if (data->input.pos == data->input.size) {
			int l = safe_read(data->fdread, data->buf,
			                  ELINKS_ZSTD_BUFFER_LENGTH);

			if (l == -1) {
				if (errno == EAGAIN)
					break;
				else
					return -1; /* I/O error */
			} else if (l == 0) {
				/* EOF. It is error: we wait for more bytes */
				return -1;
			}

			data->input.src = data->buf;
			data->input.size = l;
			data->input.pos = 0;
		}

		err = ZSTD_decompressStream(data->zstd_stream, &data->output , &data->input);

		if (ZSTD_isError(err)) {
			break;
		}
	} while (data->input.pos < data->input.size);

	if (!err) {
		data->last_read = 1;
	}

	return data->output.pos;
}

static unsigned char *
zstd_decode_buffer(struct stream_encoded *st, unsigned char *data, int len, int *new_len)
{
	struct zstd_enc_data *enc_data = (struct zstd_enc_data *)st->data;
	int error;

	*new_len = 0;	  /* default, left there if an error occurs */

	enc_data->input.src = data;
	enc_data->input.pos = 0;
	enc_data->input.size = len;
	enc_data->output.pos = 0;
	enc_data->output.size = 0;
	enc_data->output.dst = NULL;

	do {
		unsigned char *new_buffer;
		size_t size = enc_data->output.size + ELINKS_ZSTD_BUFFER_LENGTH;

		new_buffer = mem_realloc(enc_data->output.dst, size);
		if (!new_buffer) {
			error = 1;
			break;
		}

		enc_data->output.dst  = new_buffer;
		enc_data->output.size += ELINKS_ZSTD_BUFFER_LENGTH;

		error = ZSTD_decompressStream(enc_data->zstd_stream, &enc_data->output , &enc_data->input);

		if (ZSTD_isError(error)) {
			mem_free_if(enc_data->output.dst);
			enc_data->output.dst = NULL;
			return NULL;
		}
	} while (enc_data->input.pos < enc_data->input.size);

	*new_len = enc_data->output.pos;
	return enc_data->output.dst;
}

static void
zstd_close(struct stream_encoded *stream)
{
	struct zstd_enc_data *data = (struct zstd_enc_data *) stream->data;

	if (data) {
		if (data->zstd_stream) {
			ZSTD_freeDCtx(data->zstd_stream);
			data->zstd_stream = NULL;
		}
		if (data->fdread != -1) {
			close(data->fdread);
		}
		mem_free(data);
		stream->data = 0;
	}
}

static const unsigned char *const zstd_extensions[] = { ".zst", NULL };

const struct decoding_backend zstd_decoding_backend = {
	"zstd",
	zstd_extensions,
	zstd_open,
	zstd_read,
	zstd_decode_buffer,
	zstd_close,
};
