/* lzma encoding (ENCODING_LZMA) backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <lzma.h>
#include <errno.h>

#include "elinks.h"

#include "encoding/encoding.h"
#include "encoding/lzma.h"
#include "util/memory.h"

#define ELINKS_BZ_BUFFER_LENGTH 5000

#define ELINKS_LZMA_MEMORY_LIMIT (1024 * 1024 * 128)

struct lzma_enc_data {
	lzma_stream flzma_stream;
	int fdread;
	int last_read:1;
	int after_end:1;
	unsigned char buf[ELINKS_BZ_BUFFER_LENGTH];
};

static int
lzma_open(struct stream_encoded *stream, int fd)
{
	struct lzma_enc_data *data = mem_alloc(sizeof(*data));
	int err;

	stream->data = NULL;
	if (!data) {
		return -1;
	}

	memset(&data->flzma_stream, 0, sizeof(data->flzma_stream));
	data->fdread = fd;
	data->last_read = 0;

	err = lzma_auto_decoder(&data->flzma_stream, ELINKS_LZMA_MEMORY_LIMIT, 0);
	if (err != LZMA_OK) {
		mem_free(data);
		return -1;
	}

	stream->data = data;

	return 0;
}

static int
lzma_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct lzma_enc_data *data = (struct lzma_enc_data *) stream->data;
	int err = 0;

	if (!data) return -1;

	assert(len > 0);

	if (data->last_read) return 0;

	data->flzma_stream.avail_out = len;
	data->flzma_stream.next_out = buf;

	do {
		if (data->flzma_stream.avail_in == 0) {
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

			data->flzma_stream.next_in = data->buf;
			data->flzma_stream.avail_in = l;
		}

		err = lzma_code(&data->flzma_stream, LZMA_RUN);
		if (err == LZMA_STREAM_END) {
			data->last_read = 1;
			break;
		} else if (err != LZMA_OK) {
			return -1;
		}
	} while (data->flzma_stream.avail_out > 0);

	assert(len - data->flzma_stream.avail_out == data->flzma_stream.next_out - buf);
	return len - data->flzma_stream.avail_out;
}

static unsigned char *
lzma_decode_buffer(struct stream_encoded *st, unsigned char *data, int len, int *new_len)
{
	struct lzma_enc_data *enc_data = (struct lzma_enc_data *) st->data;
	lzma_stream *stream = &enc_data->flzma_stream;
	unsigned char *buffer = NULL;
	int error;

	*new_len = 0;	  /* default, left there if an error occurs */

	stream->next_in = data;
	stream->avail_in = len;
	stream->total_out = 0;

	if (lzma_auto_decoder(stream, ELINKS_LZMA_MEMORY_LIMIT, 0) != LZMA_OK)
		return NULL;

	do {
		unsigned char *new_buffer;
		size_t size = stream->total_out + MAX_STR_LEN;

		new_buffer = mem_realloc(buffer, size);
		if (!new_buffer) {
			error = LZMA_MEM_ERROR;
			break;
		}

		buffer		 = new_buffer;
		stream->next_out  = buffer + stream->total_out;
		stream->avail_out = MAX_STR_LEN;

		error = lzma_code(stream, LZMA_RUN);
		if (error == LZMA_STREAM_END) {
			error = LZMA_OK;
			break;
		}
	} while (error == LZMA_OK && stream->avail_in > 0);

	if (error == LZMA_STREAM_END) {
		lzma_end(stream);
		enc_data->after_end = 1;
		error = LZMA_OK;
	}

	if (error == LZMA_OK) {
		*new_len = stream->total_out;
		return buffer;
	} else {
		if (buffer) mem_free(buffer);
		return NULL;
	}
}

static void
lzma_close(struct stream_encoded *stream)
{
	struct lzma_enc_data *data = (struct lzma_enc_data *) stream->data;

	if (data) {
		if (!data->after_end) {
			lzma_end(&data->flzma_stream);
		}
		if (data->fdread != -1) {
			close(data->fdread);
		}
		mem_free(data);
		stream->data = 0;
	}
}

static const unsigned char *const lzma_extensions[] = { ".lzma", ".xz", NULL };

const struct decoding_backend lzma_decoding_backend = {
	"lzma",
	lzma_extensions,
	lzma_open,
	lzma_read,
	lzma_decode_buffer,
	lzma_close,
};
