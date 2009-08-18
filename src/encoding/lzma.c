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

struct lzma_enc_data {
	lzma_stream flzma_stream;
	int fdread;

	/** Error code to be returned by all later lzma_read() calls.
	 * ::READENC_EAGAIN is used here as a passive value that means
	 * no such error occurred yet.  ::READENC_ERRNO is not allowed
	 * because there is no @c sticky_errno member here.  */
	enum read_encoded_result sticky_result;

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
	
	copy_struct(&data->flzma_stream, &LZMA_STREAM_INIT_VAR);
	data->fdread = fd;
	data->sticky_result = READENC_EAGAIN;

	err = lzma_auto_decoder(&data->flzma_stream, NULL, NULL);
	if (err != LZMA_OK) {
		mem_free(data);
		return -1;
	}

	stream->data = data;

	return 0;
}

static enum read_encoded_result
map_lzma_ret(lzma_ret ret)
{
	switch (ret) {
	case LZMA_STREAM_END:
		return READENC_STREAM_END;
	case LZMA_DATA_ERROR:
	case LZMA_HEADER_ERROR:
		return READENC_DATA_ERROR;
	case LZMA_MEM_ERROR:
		return READENC_MEM_ERROR;
	case LZMA_PROG_ERROR:
	case LZMA_BUF_ERROR:
	default:
		return READENC_INTERNAL;
	}
}

/*! @return A positive number means that many bytes were
 * written to the @a buf array.  Otherwise, the value is
 * enum read_encoded_result.  */
static int
lzma_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct lzma_enc_data *data = (struct lzma_enc_data *) stream->data;
	int err = 0;
	int l = 0;

	if (!data) return READENC_INTERNAL;

	assert(len > 0);
	if_assert_failed return READENC_INTERNAL;

	if (data->sticky_result != READENC_EAGAIN)
		return data->sticky_result;

	data->flzma_stream.avail_out = len;
	data->flzma_stream.next_out = buf;

	do {
		if (data->flzma_stream.avail_in == 0) {
			l = safe_read(data->fdread, data->buf,
				      ELINKS_BZ_BUFFER_LENGTH);

			if (l == -1) {
				if (errno == EAGAIN)
					break;
				else
					return READENC_ERRNO; /* I/O error */
			} else if (l == 0) {
				/* EOF. It is error: we wait for more bytes */
				return READENC_UNEXPECTED_EOF;
			}

			data->flzma_stream.next_in = data->buf;
			data->flzma_stream.avail_in = l;
		}

		err = lzma_code(&data->flzma_stream, LZMA_RUN);
		if (err == LZMA_STREAM_END) {
			data->sticky_result = READENC_STREAM_END;
			break;
		} else if (err != LZMA_OK && err != LZMA_UNSUPPORTED_CHECK) {
			return map_lzma_ret(err);
		}
	} while (data->flzma_stream.avail_out > 0);

	l = len - data->flzma_stream.avail_out;
	assert(l == data->flzma_stream.next_out - buf);
	if (l > 0) /* Positive return values are byte counts */
		return l;
	else	   /* and others are from enum read_encoded_result */
		return data->sticky_result;
}

static unsigned char *
lzma_decode_buffer(unsigned char *data, int len, int *new_len)
{
	lzma_stream stream = LZMA_STREAM_INIT;
	unsigned char *buffer = NULL;
	int error;

	*new_len = 0;	  /* default, left there if an error occurs */

	stream.next_in = data;
	stream.avail_in = len;

	if (lzma_auto_decoder(&stream, NULL, NULL) != LZMA_OK)
		return NULL;

	do {
		unsigned char *new_buffer;
		size_t size = stream.total_out + MAX_STR_LEN;

		new_buffer = mem_realloc(buffer, size);
		if (!new_buffer) {
			error = LZMA_MEM_ERROR;
			break;
		}

		buffer		 = new_buffer;
		stream.next_out  = buffer + stream.total_out;
		stream.avail_out = MAX_STR_LEN;

		error = lzma_code(&stream, LZMA_RUN);
		if (error == LZMA_STREAM_END) {
			error = LZMA_OK;
			break;
		}
	} while (error == LZMA_OK && stream.avail_in > 0);

	lzma_end(&stream);

	if (error == LZMA_OK) {
		*new_len = stream.total_out;
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
		lzma_end(&data->flzma_stream);
		close(data->fdread);
		mem_free(data);
		stream->data = 0;
	}
}

static const unsigned char *const lzma_extensions[] = { ".lzma", NULL };

const struct decoding_backend lzma_decoding_backend = {
	"lzma",
	lzma_extensions,
	lzma_open,
	lzma_read,
	lzma_decode_buffer,
	lzma_close,
};
