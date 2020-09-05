/* zstd encoding (ENCODING_ZSTD) backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <zstd.h>
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
	size_t sent_pos;
	/* The file descriptor from which we read.  */
	int fdread;
	int decoded:1;
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

static int
zstd_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct zstd_enc_data *data = (struct zstd_enc_data *) stream->data;

	if (!data) return -1;

	assert(len > 0);

	if (!data->decoded) {
		size_t read_pos = 0;
		unsigned char *tmp_buf = malloc(len);
		int new_len;

		if (!tmp_buf) {
			return 0;
		}
		do {
			int l = safe_read(data->fdread, tmp_buf + read_pos, len - read_pos);

			if (!l) break;

			if (l == -1 && errno == EAGAIN) {
				continue;
			}
			if (l == -1) {
				return -1;
			}
			read_pos += l;
		} while (1);

		if (zstd_decode_buffer(stream, tmp_buf, len, &new_len)) {
			data->decoded = 1;
		}
		mem_free(tmp_buf);
	}
	if (data->decoded) {
		int length = len < (data->output.pos - data->sent_pos) ? len : (data->output.pos - data->sent_pos);

		if (length <= 0) {
			mem_free(data->output.dst);
			data->output.dst = NULL;
		} else {
			memcpy(buf, data->output.dst + data->sent_pos, length);
			data->sent_pos += length;
		}

		return length;
	}

	return -1;
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
