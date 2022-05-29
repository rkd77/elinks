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
	uint8_t *buffer;
	size_t sent_pos;

	/* The file descriptor from which we read.  */
	int fdread;
	int decoded:1;
	int after_end:1;
	int last_read:1;
	unsigned char buf[ELINKS_BROTLI_BUFFER_LENGTH];
};

static int
brotli_open(struct stream_encoded *stream, int fd)
{
	struct br_enc_data *data = (struct br_enc_data *)mem_calloc(1, sizeof(*data));

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
static char *brotli_decode_buffer(struct stream_encoded *st, char *datac, int len, int *new_len);

static int
brotli_read(struct stream_encoded *stream, char *buf, int len)
{
	struct br_enc_data *data = (struct br_enc_data *) stream->data;

	if (!data) return -1;

	assert(len > 0);

	if (!data->decoded) {
		size_t read_pos = 0;
		char *tmp_buf = (char *)mem_alloc(len);
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

		if (brotli_decode_buffer(stream, tmp_buf, len, &new_len)) {
			data->decoded = 1;
		}
		mem_free(tmp_buf);
	}
	if (data->decoded) {
		int length = len < (data->total_out - data->sent_pos) ? len : (data->total_out - data->sent_pos);

		if (length <= 0) {
			mem_free_set(&data->buffer, NULL);
		} else {
			memcpy(buf, (void *)((char *)(data->buffer) + data->sent_pos), length);
			data->sent_pos += length;
		}

		return length;
	}

	return -1;
}

static char *
brotli_decode_buffer(struct stream_encoded *st, char *datac, int len, int *new_len)
{
	uint8_t *data = (uint8_t *)datac;

	struct br_enc_data *enc_data = (struct br_enc_data *)st->data;
	BrotliDecoderState *state = enc_data->state;
	int error;

	*new_len = 0;	  /* default, left there if an error occurs */

	if (!len) return NULL;

	enc_data->next_in = data;
	enc_data->avail_in = len;

	do {
		char *new_buffer;
		size_t size = enc_data->total_out + ELINKS_BROTLI_BUFFER_LENGTH;
		new_buffer = (char *)mem_realloc(enc_data->buffer, size);

		if (!new_buffer) {
			error = BROTLI_DECODER_RESULT_ERROR;
			break;
		}

		enc_data->buffer		 = (uint8_t *)new_buffer;
		enc_data->next_out  = enc_data->buffer + enc_data->total_out;
		enc_data->avail_out = ELINKS_BROTLI_BUFFER_LENGTH;

		error = BrotliDecoderDecompressStream(state, &enc_data->avail_in, &enc_data->next_in,
		&enc_data->avail_out, &enc_data->next_out, &enc_data->total_out);

		if (error == BROTLI_DECODER_RESULT_SUCCESS) {
			*new_len = enc_data->total_out;
			enc_data->after_end = 1;
			return (char *)enc_data->buffer;
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

const char *
get_brotli_version(void)
{
	static char version[16];

	if (!version[0]) {
		int v = BrotliDecoderVersion();
		int major = v >> 24;
		int minor = (v >> 12) & 0xFFF;
		int patch = v & 0xFFF;

		snprintf(version, 15, "%d.%d.%d", major, minor, patch);
	}

	return version;
}

static const char *const brotli_extensions[] = { ".br", NULL };

const struct decoding_backend brotli_decoding_backend = {
	"brotli",
	brotli_extensions,
	brotli_open,
	brotli_read,
	brotli_decode_buffer,
	brotli_close,
};
