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

#include "elinks.h"

#include "encoding/bzip2.h"
#include "encoding/encoding.h"
#include "util/memory.h"

struct bz2_enc_data {
	FILE *file;
	BZFILE *bzfile;
	int last_read; /* If err after last bzRead() was BZ_STREAM_END.. */
};

struct bzFile {
	FILE *handle;
	char buf[BZ_MAX_UNUSED];
	int bufN;
	unsigned char writing;
	bz_stream strm;
	int lastErr;
	unsigned char initialisedOk;
};

/* TODO: When it'll be official, use bzdopen() from Yoshioka Tsuneo. --pasky */

static int
bzip2_open(struct stream_encoded *stream, int fd)
{
	struct bz2_enc_data *data = mem_alloc(sizeof(*data));
	int err;

	if (!data) {
		return -1;
	}
	data->last_read = 0;

	data->file = fdopen(fd, "rb");

	data->bzfile = BZ2_bzReadOpen(&err, data->file, 0, 0, NULL, 0);
	if (!data->bzfile) {
		mem_free(data);
		return -1;
	}

	stream->data = data;

	return 0;
}

static unsigned char
myfeof(FILE *f)
{
	int c = fgetc (f);

	if (c == EOF) return 1;
	ungetc(c, f);
	return 0;
}

#define BZ_SETERR(eee)                    \
{                                         \
   if (bzerror != NULL) *bzerror = eee;   \
   if (bzf != NULL) bzf->lastErr = eee;   \
}

static int
BZ2_bzRead2(int *bzerror, BZFILE *b, void *buf, int len)
{
	int n, ret, pi = 0;
	struct bzFile *bzf = (struct bzFile *)b;

	BZ_SETERR(BZ_OK);

	if (bzf == NULL || buf == NULL || len < 0) {
		BZ_SETERR(BZ_PARAM_ERROR);
		return 0;
	}

	if (bzf->writing) {
		BZ_SETERR(BZ_SEQUENCE_ERROR);
		return 0;
	}

	if (len == 0) {
		BZ_SETERR(BZ_OK);
		return 0;
	}

	bzf->strm.avail_out = len;
	bzf->strm.next_out = buf;

	while (1) {
		if (ferror(bzf->handle)) {
			BZ_SETERR(BZ_IO_ERROR);
			return 0;
		}

		if (bzf->strm.avail_in == 0 && !myfeof(bzf->handle)) {
			n = fread(bzf->buf, 1, BZ_MAX_UNUSED, bzf->handle);
			if (ferror(bzf->handle)) {
				if (n < 0) {
					BZ_SETERR(BZ_IO_ERROR);
					return 0;
				} else {
					BZ_SETERR(BZ_OK);
					pi = 1;
				}
			}
			bzf->bufN = n;
			bzf->strm.avail_in = bzf->bufN;
			bzf->strm.next_in = bzf->buf;
		}

		ret = BZ2_bzDecompress ( &(bzf->strm) );
		if (ret != BZ_OK && ret != BZ_STREAM_END) {
			BZ_SETERR(ret);
			return 0;
		}

		if (ret == BZ_OK && myfeof(bzf->handle) &&
			bzf->strm.avail_in == 0 && bzf->strm.avail_out > 0) {
			if (!pi) {
				BZ_SETERR(BZ_UNEXPECTED_EOF);
				return 0;
			} else {
				return len - bzf->strm.avail_out;
			}
		}

		if (ret == BZ_STREAM_END) {
			BZ_SETERR(BZ_STREAM_END);
			return len - bzf->strm.avail_out;
		}

		if (bzf->strm.avail_out == 0) {
			BZ_SETERR(BZ_OK);
			return len;
		}
	}
	return 0; /*not reached*/
}
#undef BZ_STRERR

static int
bzip2_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct bz2_enc_data *data = (struct bz2_enc_data *) stream->data;
	int err = 0;
	struct bzFile *bzf = (struct bzFile *)data->bzfile;

	if (data->last_read)
		return 0;

	clearerr(bzf->handle);
	len = BZ2_bzRead2(&err, data->bzfile, buf, len);

	if (err == BZ_STREAM_END)
		data->last_read = 1;
	else if (err)
		return -1;

	return len;
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
	int err;

	BZ2_bzReadClose(&err, data->bzfile);
	fclose(data->file);
	mem_free(data);
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
