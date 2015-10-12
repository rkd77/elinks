/* Brotli encoding (ENCODING_BROTLI) backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_BROTLI_DEC_DECODE_H
#include <brotli/dec/decode.h>
#endif

#include <errno.h>

#include "elinks.h"

#include "encoding/brotli.h"
#include "encoding/encoding.h"
#include "util/math.h"
#include "util/memory.h"

struct br_enc_data {
	BrotliState br_stream;

	uint8_t *input;
	uint8_t *output;

	size_t input_length;
	size_t output_length;
	size_t output_pos;
	size_t input_pos;

	/* The file descriptor from which we read.  */
	int fdread;
	int after_end:1;
	int need_free:1;
};

static int
brotli_open(struct stream_encoded *stream, int fd)
{
	struct br_enc_data *data = mem_calloc(1, sizeof(*data));

	stream->data = NULL;
	if (!data) {
		return -1;
	}

	data->fdread = fd;
	BrotliStateInit(&data->br_stream);
	stream->data = data;

	return 0;
}

static int
brotli_read_function_fd(void *data, uint8_t *buf, size_t len)
{
	struct br_enc_data *enc_data = (struct br_enc_data *)data;

	return safe_read(enc_data->fdread, buf, len);
}

static int
brotli_read_function(void *data, uint8_t *buf, size_t len)
{
	struct br_enc_data *enc_data = (struct br_enc_data *)data;
	size_t l = MIN(len, enc_data->input_length - enc_data->input_pos);

	memcpy(buf, enc_data->input + enc_data->input_pos, l);
	enc_data->input_pos += l;
	return l;
}

static int
brotli_write_function(void *data, const uint8_t *buf, size_t len)
{
	struct br_enc_data *enc_data = (struct br_enc_data *)data;

	enc_data->output = mem_alloc(len);
	if (!enc_data->output) {
		return -1;
	}
	memcpy(enc_data->output, buf, len);
	enc_data->output_length = len;
	return len;
}

static int
brotli_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct br_enc_data *enc_data = (struct br_enc_data *) stream->data;
	BrotliState *s;
	BrotliInput inp;
	BrotliOutput outp;
	size_t l;
	int error;

	if (!enc_data) return -1;
	s = &enc_data->br_stream;

	assert(len > 0);

	if (enc_data->after_end) {
		l = MIN(len, enc_data->output_length - enc_data->output_pos);
		memcpy(buf, enc_data->output + enc_data->output_pos, l);
		enc_data->output_pos += l;
		return l;
	}

	enc_data->input = NULL;
	enc_data->input_length = 0;
	enc_data->output = NULL;
	enc_data->output_length = 0;
	enc_data->output_pos = 0;
	inp.data_ = enc_data;
	outp.data_ = enc_data;
	inp.cb_ = brotli_read_function_fd;
	outp.cb_ = brotli_write_function;

	error = BrotliDecompressStreaming(inp, outp, 1, s);
	switch (error) {
	case BROTLI_RESULT_ERROR:
		return -1;
	case BROTLI_RESULT_SUCCESS:
		enc_data->after_end = 1;
	case BROTLI_RESULT_NEEDS_MORE_INPUT:
	default:
		enc_data->need_free = 1;
		l = MIN(len, enc_data->output_length - enc_data->output_pos);
		memcpy(buf, enc_data->output + enc_data->output_pos, l);
		enc_data->output_pos += l;
		return l;
	}
}

static unsigned char *
brotli_decode_buffer(struct stream_encoded *st, unsigned char *data, int len, int *new_len)
{
	struct br_enc_data *enc_data = (struct br_enc_data *)st->data;
	BrotliInput inp;
	BrotliOutput outp;
	BrotliState *stream = &enc_data->br_stream;
	int error;
	int finish = (len == 0);

	*new_len = 0;	  /* default, left there if an error occurs */
	enc_data->input = data;
	enc_data->input_length = len;
	enc_data->input_pos = 0;
	enc_data->output = NULL;
	enc_data->output_length = 0;
	enc_data->output_pos = 0;
	inp.data_ = enc_data;
	outp.data_ = enc_data;
	inp.cb_ = brotli_read_function;
	outp.cb_ = brotli_write_function;
	error = BrotliDecompressStreaming(inp, outp, finish, stream);

	switch (error) {
	case BROTLI_RESULT_ERROR:
		return NULL;
	case BROTLI_RESULT_SUCCESS:
		enc_data->after_end = 1;
	case BROTLI_RESULT_NEEDS_MORE_INPUT:
	default:
		*new_len = enc_data->output_length;
		return enc_data->output;
	}
}

static void
brotli_close(struct stream_encoded *stream)
{
	struct br_enc_data *data = (struct br_enc_data *) stream->data;

	if (data) {
		BrotliStateCleanup(&data->br_stream);
		if (data->fdread != -1) {
			close(data->fdread);
		}
		if (data->need_free) {
			mem_free_if(data->output);
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
