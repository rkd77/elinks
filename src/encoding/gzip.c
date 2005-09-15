/* Gzip encoding (ENCODING_GZIP) backend */
/* $Id: gzip.c,v 1.11 2005/04/13 15:29:23 jonas Exp $ */

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

#include "elinks.h"

#include "encoding/encoding.h"
#include "encoding/gzip.h"
#include "osdep/osdep.h"
#include "util/memory.h"


static int
gzip_open(struct stream_encoded *stream, int fd)
{
	stream->data = (void *) gzdopen(fd, "rb");
	if (!stream->data) return -1;

	return 0;
}

static int
gzip_read(struct stream_encoded *stream, unsigned char *data, int len)
{
	return gzread((gzFile *) stream->data, data, len);
}

static unsigned char *
gzip_decode(struct stream_encoded *stream, unsigned char *data, int len,
	    int *new_len)
{
	*new_len = len;
	return data;
}


/* The following code for decoding gzip in memory is a mix of code from zlib's
 * gzio.c file copyrighted 1995-2002 by Jean-loup Gailly and the costumized
 * header extraction in the linux kernels lib/inflate.c file not copyrighted
 * 1992 by Mark Adler. */

static int gzip_header_magic[2] = { 0x1f, 0x8b };

enum gzip_header_flag {
	GZIP_ASCII_TEXT	 = 0x01, /* File probably ascii text (unused) */
	GZIP_HEADER_CRC	 = 0x02, /* Header CRC present */
	GZIP_EXTRA_FIELD = 0x04, /* Extra field present */
	GZIP_ORIG_NAME	 = 0x08, /* Original file name present */
	GZIP_COMMENT	 = 0x10, /* File comment present */
	GZIP_RESERVED	 = 0xE0, /* bits 5..7: reserved */
};

/* Read a byte from a gz_stream; update next_in and avail_in. Return EOF for
 * end of file. */
static int
get_gzip_byte(z_stream *stream)
{
	if (stream->avail_in == 0)
		return EOF;

	stream->avail_in--;

	return *(stream->next_in)++;
}

#define skip_gzip_bytes(stream, bytes) \
	do { int i = bytes; while (i-- > 0) get_gzip_byte(stream); } while (0)

#define skip_gzip_string(stream) \
	do { int i; while ((i = get_gzip_byte(stream)) != 0 && i != EOF) ; } while (0)

/* Check the gzip header of a gz_stream opened for reading. Set the stream mode
 * to transparent if the gzip magic header is not present; set s->err to
 * Z_DATA_ERROR if the magic header is present but the rest of the header is
 * incorrect. */
static int
skip_gzip_header(z_stream *stream)
{
	unsigned int len;
	int method; /* method byte */
	int flags;  /* flags byte */

	/* Check the gzip magic header */
	for (len = 0; len < 2; len++) {
		int byte = get_gzip_byte(stream);

		if (byte != gzip_header_magic[len]) {
			if (len != 0) {
				stream->avail_in++;
				stream->next_in--;
			}

			if (byte != EOF) {
				stream->avail_in++;
				stream->next_in--;
			}

			return stream->avail_in != 0 ? Z_OK : Z_STREAM_END;
		}
	}

	method = get_gzip_byte(stream);
	flags = get_gzip_byte(stream);

	if (method != Z_DEFLATED || (flags & GZIP_RESERVED) != 0)
		return Z_DATA_ERROR;

	/* Discard time, xflags and OS code: */
	skip_gzip_bytes(stream, 6);

	if (flags & GZIP_EXTRA_FIELD) {
		/* Skip the extra field */
		len  =  (unsigned int) get_gzip_byte(stream);
		len += ((unsigned int) get_gzip_byte(stream)) << 8;

		/* If EOF is encountered @len is garbage, but the loop below
		 * will quit anyway. */
		while (len-- > 0 && get_gzip_byte(stream) != EOF) ;
	}

	/* Skip the original file name */
	if (flags & GZIP_ORIG_NAME)
		skip_gzip_string(stream);

	/* Skip the .gz file comment */
	if (flags & GZIP_COMMENT)
		skip_gzip_string(stream);

	/* Skip the header CRC */
	if (flags & GZIP_HEADER_CRC)
		skip_gzip_bytes(stream, 2);

	return Z_OK;
}


/* Freaking dammit. This is impossible for me to get working. */
static unsigned char *
gzip_decode_buffer(unsigned char *data, int len, int *new_len)
{
	unsigned char *buffer = NULL;
	int error = Z_OK;
	int tries, wbits;

	/* This WBITS loop thing was something I got from
	 * http://lists.infradead.org/pipermail/linux-mtd/2002-March/004429.html
	 * but it doesn't fix it. :/ --jonas */
	/* -MAX_WBITS impiles -> suppress zlib header and adler32.  try first
	 * with -MAX_WBITS, if that fails, try MAX_WBITS to be backwards
	 * compatible */
	wbits = -MAX_WBITS;

	for (tries = 0; tries < 2; tries++) {
		z_stream stream;

		memset(&stream, 0, sizeof(z_stream));

		/* FIXME: Use inflateInit2() to configure low memory
		 * usage for CONFIG_SMALL configurations. --jonas */
		error = inflateInit2(&stream, wbits);
		if (error != Z_OK) break;

		stream.next_in = (char *)data;
		stream.avail_in = len;

		error = skip_gzip_header(&stream);
		if (error != Z_OK) {
			stream.next_in = (char *)data;
			stream.avail_in = len;
		}

		do {
			unsigned char *new_buffer;
			size_t size = stream.total_out + MAX_STR_LEN;

			assert(stream.total_out >= 0);
			assert(stream.next_in);

			new_buffer = mem_realloc(buffer, size);
			if (!new_buffer) {
				error = Z_MEM_ERROR;
				break;
			}

			buffer		 = new_buffer;
			stream.next_out  = buffer + stream.total_out;
			stream.avail_out = MAX_STR_LEN;

			error = inflate(&stream, Z_NO_FLUSH);
			if (error == Z_STREAM_END) {
				/* Here gzio.c has some detection of
				 * concatenated .gz files and will do a gzip
				 * header skip and an inflateReset() call
				 * before continuing. It partly uses CRC to
				 * detect that. */
				*new_len = stream.total_out;
				error = Z_OK;
				break;
			}

		} while (error == Z_OK && stream.avail_in > 0);

		inflateEnd(&stream);

		if (error != Z_DATA_ERROR)
			break;

		/* Try again with next wbits */
		wbits = -wbits;
	}

	if (error != Z_OK) {
		if (buffer) mem_free(buffer);
		*new_len = 0;
		return NULL;
	}

	return buffer;
}


static void
gzip_close(struct stream_encoded *stream)
{
	gzclose((gzFile *) stream->data);
}

static unsigned char *gzip_extensions[] = { ".gz", ".tgz", NULL };

struct decoding_backend gzip_decoding_backend = {
	"gzip",
	gzip_extensions,
	gzip_open,
	gzip_read,
	gzip_decode,
	gzip_decode_buffer,
	gzip_close,
};
