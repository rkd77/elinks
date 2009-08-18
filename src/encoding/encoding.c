/* Stream reading and decoding (mostly decompression) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "encoding/encoding.h"
#include "network/state.h"
#include "osdep/osdep.h"
#include "util/memory.h"
#include "util/string.h"


/*************************************************************************
  Dummy encoding (ENCODING_NONE)
*************************************************************************/

struct dummy_enc_data {
	int fd;
};

static int
dummy_open(struct stream_encoded *stream, int fd)
{
	stream->data = mem_alloc(sizeof(struct dummy_enc_data));
	if (!stream->data) return -1;

	((struct dummy_enc_data *) stream->data)->fd = fd;

	return 0;
}

/*! @return A positive number means that many bytes were
 * written to the @a data array.  Otherwise, the value is
 * enum read_encoded_result.  */
static int
dummy_read(struct stream_encoded *stream, unsigned char *data, int len)
{
	struct dummy_enc_data *const enc = stream->data;
	int got = safe_read(enc->fd, data, len);

	if (got > 0)
		return got;
	else if (got == 0)
		return READENC_STREAM_END;
	else if (errno == EAGAIN)
		return READENC_EAGAIN;
	else
		return READENC_ERRNO;
}

static unsigned char *
dummy_decode_buffer(unsigned char *data, int len, int *new_len)
{
	unsigned char *buffer = memacpy(data, len);

	if (!buffer) return NULL;

	*new_len = len;
	return buffer;
}

static void
dummy_close(struct stream_encoded *stream)
{
	close(((struct dummy_enc_data *) stream->data)->fd);
	mem_free(stream->data);
}

static const unsigned char *const dummy_extensions[] = { NULL };

static const struct decoding_backend dummy_decoding_backend = {
	"none",
	dummy_extensions,
	dummy_open,
	dummy_read,
	dummy_decode_buffer,
	dummy_close,
};


/* Dynamic backend area */

#include "encoding/bzip2.h"
#include "encoding/deflate.h"
#include "encoding/lzma.h"

static const struct decoding_backend *const decoding_backends[] = {
	&dummy_decoding_backend,
	&gzip_decoding_backend,
	&bzip2_decoding_backend,
	&lzma_decoding_backend,
	&deflate_decoding_backend,
};


/*************************************************************************
  Public functions
*************************************************************************/


/* Associates encoded stream with a fd. */
struct stream_encoded *
open_encoded(int fd, enum stream_encoding encoding)
{
	struct stream_encoded *stream;

	stream = mem_alloc(sizeof(*stream));
	if (!stream) return NULL;

	stream->encoding = encoding;
	if (decoding_backends[stream->encoding]->open(stream, fd) >= 0)
		return stream;

	mem_free(stream);
	return NULL;
}

/** Read available data from stream and decode them. Note that when
 * data change their size during decoding, @a len indicates desired
 * size of _returned_ data, not desired size of data read from
 * stream.
 *
 * @return A positive number means that many bytes were written to the
 * @a data array.  Otherwise, the value is enum read_encoded_result.  */
int
read_encoded(struct stream_encoded *stream, unsigned char *data, int len)
{
	return decoding_backends[stream->encoding]->read(stream, data, len);
}

/* Decode an entire file from a buffer. This function is not suitable
 * for parts of files. @data contains the original data, @len bytes
 * long. The resulting decoded data chunk is *@new_len bytes long. */
unsigned char *
decode_encoded_buffer(enum stream_encoding encoding, unsigned char *data, int len,
		      int *new_len)
{
	return decoding_backends[encoding]->decode_buffer(data, len, new_len);
}

/* Closes encoded stream. Note that fd associated with the stream will be
 * closed here. */
void
close_encoded(struct stream_encoded *stream)
{
	decoding_backends[stream->encoding]->close(stream);
	mem_free(stream);
}


/* Return a list of extensions associated with that encoding. */
const unsigned char *const *listext_encoded(enum stream_encoding encoding)
{
	return decoding_backends[encoding]->extensions;
}

enum stream_encoding
guess_encoding(unsigned char *filename)
{
	int fname_len = strlen(filename);
	unsigned char *fname_end = filename + fname_len;
	int enc;

	for (enc = 1; enc < ENCODINGS_KNOWN; enc++) {
		const unsigned char *const *ext = decoding_backends[enc]->extensions;

		while (ext && *ext) {
			int len = strlen(*ext);

			if (fname_len >= len && !strcmp(fname_end - len, *ext))
				return enc;

			ext++;
		}
	}

	return ENCODING_NONE;
}

const unsigned char *
get_encoding_name(enum stream_encoding encoding)
{
	return decoding_backends[encoding]->name;
}


/* File reading */

/* Tries to open @prefixname with each of the supported encoding extensions
 * appended. */
static inline enum stream_encoding
try_encoding_extensions(struct string *filename, int *fd)
{
	int length = filename->length;
	int encoding;

	/* No file of that name was found, try some others names. */
	for (encoding = 1; encoding < ENCODINGS_KNOWN; encoding++) {
		const unsigned char *const *ext = listext_encoded(encoding);

		for (; ext && *ext; ext++) {
			add_to_string(filename, *ext);

			/* We try with some extensions. */
			*fd = open(filename->source, O_RDONLY | O_NOCTTY);

			if (*fd >= 0)
				/* Ok, found one, use it. */
				return encoding;

			filename->source[length] = 0;
			filename->length = length;
		}
	}

	return ENCODING_NONE;
}

/** Reads the file from @a stream in chunks of size @a readsize.
 *
 * @a stream should be in blocking mode.  If it is in non-blocking
 * mode, this function can return an empty string in @a page just
 * because no more data is available yet, and the caller cannot know
 * whether the true end of the stream has been reached.
 *
 * @return a connection state. S_OK if all is well. */
struct connection_state
read_file(struct stream_encoded *stream, int readsize, struct string *page)
{
	int readlen;
	int save_errno;

	if (!init_string(page)) return connection_state(S_OUT_OF_MEM);

	/* We read with granularity of stt.st_size (given as @readsize) - this
	 * does best job for uncompressed files, and doesn't hurt for
	 * compressed ones anyway - very large files usually tend to inflate
	 * fast anyway. At least I hope ;).  --pasky */
	/* Also there because of bug in Linux. Read returns -EACCES when
	 * reading 0 bytes to invalid address so ensure never to try and
	 * allocate zero number of bytes. */
	if (!readsize) readsize = 4096;

	for (;;) {
		unsigned char *string_pos;
		
		if (!realloc_string(page, page->length + readsize)) {
			done_string(page);
			return connection_state(S_OUT_OF_MEM);
		}
		
		string_pos = page->source + page->length;
		readlen = read_encoded(stream, string_pos, readsize);
		if (readlen <= 0) {
			save_errno = errno; /* in case of READENC_ERRNO */
			break;
		}

		page->length += readlen;
	}

	switch (readlen) {
	case READENC_ERRNO:
		done_string(page);
		return connection_state_for_errno(save_errno);

	case READENC_STREAM_END:
		/* NUL-terminate just in case */
		page->source[page->length] = '\0';
		return connection_state(S_OK);

	case READENC_UNEXPECTED_EOF:
	case READENC_DATA_ERROR:
		done_string(page);
		/* FIXME: This is indeed an internal error. If readed from a
		 * corrupted encoded file nothing or only some of the
		 * data will be read. */
		return connection_state(S_ENCODE_ERROR);

	case READENC_MEM_ERROR:
		done_string(page);
		return connection_state(S_OUT_OF_MEM);

	case READENC_EAGAIN:
	case READENC_INTERNAL:
	default:
		ERROR("unexpected readlen==%d", readlen);
		/* If you have a breakpoint in elinks_error(),
		 * you can examine page before it gets freed.  */
		done_string(page);
		return connection_state(S_INTERNAL);
	}
}

static inline int
is_stdin_pipe(struct stat *stt, struct string *filename)
{
	/* On Mac OS X, /dev/stdin has type S_IFSOCK. (bug 616) */
	return !strlcmp(filename->source, filename->length, "/dev/stdin", 10)
		&& (
#ifdef S_ISSOCK
			S_ISSOCK(stt->st_mode) ||
#endif
			S_ISFIFO(stt->st_mode));
}

struct connection_state
read_encoded_file(struct string *filename, struct string *page)
{
	struct stream_encoded *stream;
	struct stat stt;
	enum stream_encoding encoding = ENCODING_NONE;
	int fd = open(filename->source, O_RDONLY | O_NOCTTY);
	struct connection_state state = connection_state_for_errno(errno);

	if (fd == -1 && get_opt_bool("protocol.file.try_encoding_extensions")) {
		encoding = try_encoding_extensions(filename, &fd);

	} else if (fd != -1) {
		encoding = guess_encoding(filename->source);
	}

	if (fd == -1) {
#ifdef HAVE_SYS_CYGWIN_H
		/* There is no /dev/stdin on Cygwin. */
		if (!strlcmp(filename->source, filename->length, "/dev/stdin", 10)) {
			fd = STDIN_FILENO;
		} else
#endif
		return state;
	}

	/* Some file was opened so let's get down to bi'ness */
	set_bin(fd);

	/* Do all the necessary checks before trying to read the file.
	 * @state code is used to block further progress. */
	if (fstat(fd, &stt)) {
		state = connection_state_for_errno(errno);

	} else if (!S_ISREG(stt.st_mode) && encoding != ENCODING_NONE) {
		/* We only want to open regular encoded files. */
		/* Leave @state being the saved errno */

	} else if (!S_ISREG(stt.st_mode) && !is_stdin_pipe(&stt, filename)
	           && !get_opt_bool("protocol.file.allow_special_files")) {
		state = connection_state(S_FILE_TYPE);

	} else if (!(stream = open_encoded(fd, encoding))) {
		state = connection_state(S_OUT_OF_MEM);

	} else {
		int readsize = (int) stt.st_size;

		/* Check if st_size will cause overflow. */
		/* FIXME: See bug 497 for info about support for big files. */
		if (readsize != stt.st_size || readsize < 0) {
#ifdef EFBIG
			state = connection_state_for_errno(EFBIG);
#else
			state = connection_state(S_FILE_ERROR);
#endif

		} else {
			state = read_file(stream, stt.st_size, page);
		}
		close_encoded(stream);
	}

	close(fd);
	return state;
}
