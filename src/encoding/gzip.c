/* deflate/gzip encoding backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <zlib.h>
#include <errno.h>

#include "elinks.h"

#include "encoding/encoding.h"
#include "encoding/gzip.h"

#ifdef CONFIG_DEBUG
#include "util/memcount.h"
#endif

#include "util/memory.h"

/* How many bytes of compressed data to read before decompressing. */
#define ELINKS_DEFLATE_BUFFER_LENGTH 5000

struct deflate_enc_data {
	z_stream deflate_stream;

	/* The file descriptor from which we read.  */
	int fdread;
	size_t sent_pos;
	uint8_t *buffer;

	unsigned int decoded:1;
	unsigned int last_read:1;
	unsigned int after_first_read:1;
	unsigned int after_end:1;

	/* A buffer for data that has been read from the file but not
	 * yet decompressed.  z_stream.next_in and z_stream.avail_in
	 * refer to this buffer.  */
	unsigned char buf[ELINKS_DEFLATE_BUFFER_LENGTH];
};

static int
deflate_open(int window_size, struct stream_encoded *stream, int fd)
{
	/* A zero-initialized z_stream.  The compiler ensures that all
	 * pointer members in it are null.  (Can't do this with memset
	 * because C99 does not require all-bits-zero to be a null
	 * pointer.)  */
	static const z_stream null_z_stream = {0};
	int err;

	struct deflate_enc_data *data = (struct deflate_enc_data *)mem_alloc(sizeof(*data));

	stream->data = NULL;
	if (!data) {
		return -1;
	}

	/* Initialize all members of *data, except data->buf[], which
	 * will be initialized on demand by deflate_read.  */
	copy_struct(&data->deflate_stream, &null_z_stream);
#ifdef CONFIG_DEBUG
	data->deflate_stream.zalloc = el_gzip_alloc;
	data->deflate_stream.zfree = el_gzip_free;
#endif
	data->fdread = fd;
	data->last_read = 0;
	data->after_first_read = 0;
	data->after_end = 0;
	data->sent_pos = 0;
	data->decoded = 0;

	if (window_size > 0) {
		err = inflateInit2(&data->deflate_stream, window_size);
	} else {
		err = inflateInit(&data->deflate_stream);
	}
	if (err != Z_OK) {
		mem_free(data);
		return -1;
	}
	stream->data = data;

	return 0;
}

static int
deflate_gzip_open(struct stream_encoded *stream, int fd)
{
	/* detect gzip header, else assume zlib header */
	return deflate_open(MAX_WBITS + 32, stream, fd);
}

static char *deflate_gzip_decode_buffer(struct stream_encoded *st, char *data, int len, int *new_len);

static int
deflate_read(struct stream_encoded *stream, char *buf, int len)
{
	struct deflate_enc_data *data = (struct deflate_enc_data *) stream->data;

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

		if (deflate_gzip_decode_buffer(stream, tmp_buf, len, &new_len)) {
			data->decoded = 1;
		}
		mem_free(tmp_buf);
	}
	if (data->decoded) {
		int length = len < (data->deflate_stream.total_out - data->sent_pos) ? len : (data->deflate_stream.total_out - data->sent_pos);

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
deflate_decode_buffer(struct stream_encoded *st, int window_size, char *datac, int len, int *new_len)
{
	unsigned char *data = (unsigned char *)datac;
	struct deflate_enc_data *enc_data = (struct deflate_enc_data *) st->data;
	z_stream *stream = &enc_data->deflate_stream;
	unsigned char *buffer = NULL;
	int error;

	*new_len = 0;	  /* default, left there if an error occurs */

	if (!len) return NULL;
	stream->next_in = data;
	stream->avail_in = len;
	stream->total_out = 0;

	do {
		unsigned char *new_buffer;
		size_t size = stream->total_out + MAX_STR_LEN;

		new_buffer = (unsigned char *)mem_realloc(buffer, size);
		if (!new_buffer) {
			error = Z_MEM_ERROR;
			break;
		}

		buffer		 = new_buffer;
		stream->next_out  = buffer + stream->total_out;
		stream->avail_out = MAX_STR_LEN;
restart2:
		error = inflate(stream, Z_SYNC_FLUSH);
		if (error == Z_STREAM_END) {
			break;
		}
		if (error == Z_DATA_ERROR && !enc_data->after_first_read) {
			(void)inflateEnd(stream);
			error = inflateInit2(stream, -MAX_WBITS);
			if (error == Z_OK) {
				enc_data->after_first_read = 1;
				stream->next_in = data;
				stream->avail_in = len;
				goto restart2;
			}
		}
	} while (error == Z_OK && stream->avail_in > 0);

	if (error == Z_STREAM_END) {
		inflateEnd(stream);
		enc_data->after_end = 1;
		error = Z_OK;
	}

	enc_data->buffer = (uint8_t *)buffer;
	if (error == Z_OK) {
		*new_len = stream->total_out;
		return (char *)buffer;
	} else {
		mem_free_set(&enc_data->buffer, NULL);
		return NULL;
	}
}

static char *
deflate_gzip_decode_buffer(struct stream_encoded *st, char *data, int len, int *new_len)
{
	/* detect gzip header, else assume zlib header */
	return deflate_decode_buffer(st, MAX_WBITS + 32, data, len, new_len);
}

static void
deflate_close(struct stream_encoded *stream)
{
	struct deflate_enc_data *data = (struct deflate_enc_data *) stream->data;

	if (data) {
		if (!data->after_end) {
			inflateEnd(&data->deflate_stream);
		}
		if (data->fdread != -1) {
			close(data->fdread);
		}
		mem_free(data);
		stream->data = 0;
	}
}

const char *
get_gzip_version(void)
{
	return zlibVersion();
}

static const char *const gzip_extensions[] = { ".gz", ".tgz", NULL };

const struct decoding_backend gzip_decoding_backend = {
	"gzip",
	gzip_extensions,
	deflate_gzip_open,
	deflate_read,
	deflate_gzip_decode_buffer,
	deflate_close,
};
