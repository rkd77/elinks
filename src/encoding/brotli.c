/* Brotli encoding (ENCODING_BROTLI) backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <brotli/decode.h>
#include <errno.h>

#include "elinks.h"

#include "encoding/brotli.h"
#include "encoding/encoding.h"
#include "util/math.h"
#include "util/memory.h"

#define ELINKS_BROTLI_BUFFER_LENGTH 4096

struct br_enc_data {
	BrotliDecoderState *state;

	const uint8_t *next_in;
	uint8_t *next_out;

	size_t avail_in;
	size_t avail_out;
	size_t total_out;
	unsigned char *buffer;

	/* The file descriptor from which we read.  */
	int fdread;
	int after_end:1;
	int last_read:1;
	unsigned char buf[ELINKS_BROTLI_BUFFER_LENGTH];
};

static int
brotli_open(struct stream_encoded *stream, int fd)
{
	struct br_enc_data *data = mem_calloc(1, sizeof(*data));

	stream->data = NULL;
	if (!data) {
		return -1;
	}
	data->state = BrotliDecoderCreateInstance(NULL, NULL, NULL);

	if (!data->state) {
		mem_free(data);
		return -1;
	}

	data->fdread = fd;
	stream->data = data;

	return 0;
}

static int
brotli_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct br_enc_data *data = (struct br_enc_data *) stream->data;
	int err = 0;

	if (!data) return -1;

	assert(len > 0);

	if (data->last_read) return 0;

	data->avail_out = len;
	data->next_out = buf;

	do {
		if (data->avail_in == 0) {
			int l = safe_read(data->fdread, data->buf,
			                  ELINKS_BROTLI_BUFFER_LENGTH);

			if (l == -1) {
				if (errno == EAGAIN)
					break;
				else
					return -1; /* I/O error */
			} else if (l == 0) {
				/* EOF. It is error: we wait for more bytes */
				return -1;
			}

			data->next_in = data->buf;
			data->avail_in = l;
		}

		err = BrotliDecoderDecompressStream(data->state, &data->avail_in, &data->next_in,
		&data->avail_out, &data->next_out, &data->total_out);

		if (err == BROTLI_DECODER_RESULT_SUCCESS) {
			data->last_read = 1;
			break;
		} else if (err == BROTLI_DECODER_RESULT_ERROR) {
			return -1;
		}
	} while (data->avail_out > 0);

	assert(len - data->avail_out == data->next_out - buf);
	return len - data->avail_out;
}

static unsigned char *
brotli_decode_buffer(struct stream_encoded *st, unsigned char *data, int len, int *new_len)
{
	struct br_enc_data *enc_data = (struct br_enc_data *)st->data;
	BrotliDecoderState *state = enc_data->state;
	int error;

	*new_len = 0;	  /* default, left there if an error occurs */

	if (!len) return NULL;

	enc_data->next_in = data;
	enc_data->avail_in = len;

	do {
		unsigned char *new_buffer;
		size_t size = enc_data->total_out + ELINKS_BROTLI_BUFFER_LENGTH;
		new_buffer = mem_realloc(enc_data->buffer, size);

		if (!new_buffer) {
			error = BROTLI_DECODER_RESULT_ERROR;
			break;
		}

		enc_data->buffer		 = new_buffer;
		enc_data->next_out  = enc_data->buffer + enc_data->total_out;
		enc_data->avail_out = ELINKS_BROTLI_BUFFER_LENGTH;

		error = BrotliDecoderDecompressStream(state, &enc_data->avail_in, &enc_data->next_in,
		&enc_data->avail_out, &enc_data->next_out, &enc_data->total_out);

		if (error == BROTLI_DECODER_RESULT_SUCCESS) {
			*new_len = enc_data->total_out;
			enc_data->after_end = 1;
			return enc_data->buffer;
		}
	} while (error == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);

	if (error == BROTLI_DECODER_RESULT_ERROR) {
		mem_free_if(enc_data->buffer);
		enc_data->buffer = NULL;
	}

	return NULL;
}

static void
brotli_close(struct stream_encoded *stream)
{
	struct br_enc_data *data = (struct br_enc_data *) stream->data;

	if (data) {
		if (data->state) BrotliDecoderDestroyInstance(data->state);
		if (data->fdread != -1) {
			close(data->fdread);
		}
		mem_free(data);
		stream->data = 0;
	}
}

static const unsigned char *const brotli_extensions[] = { ".br", NULL };

const struct decoding_backend brotli_decoding_backend = {
	"brotli",
	brotli_extensions,
	brotli_open,
	brotli_read,
	brotli_decode_buffer,
	brotli_close,
};
