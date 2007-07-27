/** Low-level terminal-suitable I/O routines
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "terminal/hardio.h"
#include "util/error.h"

/* Define it to have data written to stderr */
#if 0
#define DEBUG_HARDIO
#endif

#undef debug_open
#undef debug_flush
#undef debug_write

#ifdef DEBUG_HARDIO
static void
hw_debug_open(unsigned char *name, int fd, unsigned char *data, int datalen)
{
	fprintf(stderr, "[%s (fd=%d, data=%p, datalen=%d)]\n",
		name, fd, data, datalen);
}

static void
hw_debug_flush()
{
	fputs("\n\n", stderr);
	fflush(stderr);
}

static void
hw_debug_write(unsigned char *data, int w)
{
	int hex = 0;
	int i = 0;

	if (!w) return;

	for (; i < w; i++) {
		if (data[i] == ' ') {
			int c = i;

			while (i < w && data[++i] == ' ');

			if (i - c - 1 > 1) {
				fprintf(stderr, "[+ %d spaces]\n", i - c - 1);
				if (i == w) break;
				c = 0;
				continue;
			}
			c = 0;
			i--;
		}

		if (data[i] >= ' ' && data[i] < 127 && data[i] != '|') {
			if (hex) {
				fputc('|', stderr);
				hex = 0;
			}
			fputc(data[i], stderr);
		} else {
			if (!hex) {
				fputc('|', stderr);
				hex = 1;
			}
			fprintf(stderr,"%02x", data[i]);
		}
	}
}

#define debug_open(n, fd, data, datalen) hw_debug_open(n, fd, data, datalen)
#define debug_flush()			 hw_debug_flush()
#define debug_write(data, datalen)	 hw_debug_write(data, datalen)

#else /* DEBUG_HARDIO */

#define debug_open(n, fd, data, datalen)
#define debug_flush()
#define debug_write(data, datalen)

#endif /* DEBUG_HARDIO */


ssize_t
hard_write(int fd, unsigned char *data, size_t datalen)
{
	ssize_t total = datalen;

	assert(data && datalen >= 0);
	if_assert_failed return -1;

	debug_open("hard_write", fd, data, datalen);

	while (datalen > 0) {
		ssize_t written = safe_write(fd, data, datalen);

		if (written <= 0) {
			if (written) return -1;
			break;
		}

		debug_write(data, written);

		data	+= written;
		datalen	-= written;
	}

	debug_flush();

	/* Return number of bytes written. */
	return (total - datalen);
}

ssize_t
hard_read(int fd, unsigned char *data, size_t datalen)
{
	ssize_t total = datalen;

	assert(data && datalen >= 0);
	if_assert_failed return -1;

	debug_open("hard_read", fd, data, datalen);

	while (datalen > 0) {
		ssize_t readlen = safe_read(fd, data, datalen);

		if (readlen <= 0) {
			if (readlen) return -1;
			break;
		}

		debug_write(data, readlen);

		data	+= readlen;
		datalen	-= readlen;
	}

	debug_flush();

	/* Return number of bytes read. */
	return (total - datalen);
}
