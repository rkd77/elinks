/* deflate/gzip encoding backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif
#include <errno.h>

#include "elinks.h"

#include "encoding/deflate.h"
#include "encoding/encoding.h"
#include "util/memory.h"

/* How many bytes of compressed data to read before decompressing. */
#define ELINKS_DEFLATE_BUFFER_LENGTH 5000

struct deflate_enc_data {
	z_stream deflate_stream;

	/* The file descriptor from which we read.  */
	int fdread;

	unsigned int last_read:1;
	unsigned int after_first_read:1;

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

	struct deflate_enc_data *data = mem_alloc(sizeof(*data));

	stream->data = NULL;
	if (!data) {
		return -1;
	}

	/* Initialize all members of *data, except data->buf[], which
	 * will be initialized on demand by deflate_read.  */
	copy_struct(&data->deflate_stream, &null_z_stream);
	data->fdread = fd;
	data->last_read = 0;
	data->after_first_read = 0;

	err = inflateInit2(&data->deflate_stream, window_size);
	if (err != Z_OK) {
		mem_free(data);
		return -1;
	}
	stream->data = data;

	return 0;
}

#if 0
static int
deflate_raw_open(struct stream_encoded *stream, int fd)
{
	/* raw DEFLATE with neither zlib nor gzip header */
	return deflate_open(-MAX_WBITS, stream, fd);
}
#endif

static int
deflate_gzip_open(struct stream_encoded *stream, int fd)
{
	/* detect gzip header, else assume zlib header */
	return deflate_open(MAX_WBITS + 32, stream, fd);
}

static int
deflate_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct deflate_enc_data *data = (struct deflate_enc_data *) stream->data;
	int err = 0;
	int l = 0;

	if (!data) return -1;

	assert(len > 0);

	if (data->last_read) return 0;

	data->deflate_stream.avail_out = len;
	data->deflate_stream.next_out = buf;

	do {
		if (data->deflate_stream.avail_in == 0) {
			l = safe_read(data->fdread, data->buf,
			                  ELINKS_DEFLATE_BUFFER_LENGTH);

			if (l == -1) {
				if (errno == EAGAIN)
					break;
				else
					return -1; /* I/O error */
			} else if (l == 0) {
				/* EOF. It is error: we wait for more bytes */
				return -1;
			}

			data->deflate_stream.next_in = data->buf;
			data->deflate_stream.avail_in = l;
		}
restart:
		err = inflate(&data->deflate_stream, Z_SYNC_FLUSH);
		if (err == Z_DATA_ERROR && !data->after_first_read
		&& data->deflate_stream.next_out == buf) {
			/* RFC 2616 requires a zlib header for
			 * "Content-Encoding: deflate", but some HTTP
			 * servers (Microsoft-IIS/6.0 at blogs.msdn.com,
			 * and reportedly Apache with mod_deflate) omit
			 * that, causing Z_DATA_ERROR.  Clarification of
			 * the term "deflate" has been requested for the
			 * next version of HTTP:
			 * http://www3.tools.ietf.org/wg/httpbis/trac/ticket/73
			 *
			 * Try to recover by telling zlib not to expect
			 * the header.  If the error does not happen on
			 * the first inflate() call, then it is too late
			 * to recover because ELinks may already have
			 * discarded part of the input data.
			 *
			 * TODO: This fallback to raw DEFLATE is currently
			 * enabled for "Content-Encoding: gzip" too.  It
			 * might be better to fall back to no compression
			 * at all, because Apache can send that header for
			 * uncompressed *.gz.md5 files.  */
			data->after_first_read = 1;
			inflateEnd(&data->deflate_stream);
			data->deflate_stream.avail_out = len;
			data->deflate_stream.next_out = buf;
			data->deflate_stream.next_in = data->buf;
			data->deflate_stream.avail_in = l;
			err = inflateInit2(&data->deflate_stream, -MAX_WBITS);
			if (err == Z_OK) goto restart;
		}
		data->after_first_read = 1;
		if (err == Z_STREAM_END) {
			data->last_read = 1;
			break;
		} else if (err != Z_OK) {
			data->last_read = 1;
			break;
		}
	} while (data->deflate_stream.avail_out > 0);

	assert(len - data->deflate_stream.avail_out == data->deflate_stream.next_out - buf);
	return len - data->deflate_stream.avail_out;
}

static unsigned char *
deflate_decode_buffer(int window_size, unsigned char *data, int len, int *new_len)
{
	z_stream stream;
	unsigned char *buffer = NULL;
	int error;

	*new_len = 0;	  /* default, left there if an error occurs */

	if (!len) return NULL;
	memset(&stream, 0, sizeof(z_stream));
	stream.next_in = data;
	stream.avail_in = len;

	if (inflateInit2(&stream, window_size) != Z_OK)
		return NULL;

	do {
		unsigned char *new_buffer;
		size_t size = stream.total_out + MAX_STR_LEN;

		new_buffer = mem_realloc(buffer, size);
		if (!new_buffer) {
			error = Z_MEM_ERROR;
			break;
		}

		buffer		 = new_buffer;
		stream.next_out  = buffer + stream.total_out;
		stream.avail_out = MAX_STR_LEN;

		error = inflate(&stream, Z_SYNC_FLUSH);
		if (error == Z_STREAM_END) {
			error = Z_OK;
			break;
		}
	} while (error == Z_OK && stream.avail_in > 0);

	inflateEnd(&stream);

	if (error == Z_OK) {
		*new_len = stream.total_out;
		return buffer;
	} else {
		if (buffer) mem_free(buffer);
		return NULL;
	}
}

static unsigned char *
deflate_raw_decode_buffer(unsigned char *data, int len, int *new_len)
{
	/* raw DEFLATE with neither zlib nor gzip header */
	return deflate_decode_buffer(-MAX_WBITS, data, len, new_len);
}

static unsigned char *
deflate_gzip_decode_buffer(unsigned char *data, int len, int *new_len)
{
	/* detect gzip header, else assume zlib header */
	return deflate_decode_buffer(MAX_WBITS + 32, data, len, new_len);
}

static void
deflate_close(struct stream_encoded *stream)
{
	struct deflate_enc_data *data = (struct deflate_enc_data *) stream->data;

	if (data) {
		inflateEnd(&data->deflate_stream);
		close(data->fdread);
		mem_free(data);
		stream->data = 0;
	}
}

static const unsigned char *const deflate_extensions[] = { NULL };

const struct decoding_backend deflate_decoding_backend = {
	"deflate",
	deflate_extensions,
	deflate_gzip_open,
	deflate_read,
	deflate_raw_decode_buffer,
	deflate_close,
};

static const unsigned char *const gzip_extensions[] = { ".gz", ".tgz", NULL };

const struct decoding_backend gzip_decoding_backend = {
	"gzip",
	gzip_extensions,
	deflate_gzip_open,
	deflate_read,
	deflate_gzip_decode_buffer,
	deflate_close,
};
