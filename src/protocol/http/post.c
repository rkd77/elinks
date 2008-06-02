/** Parsing uri.post and uploading files in a POST request.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif

#include "elinks.h"

#include "protocol/http/post.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"

/** Initialize *@a http_post so that done_http_post() can be safely
 * called.
 *
 * @relates http_post */
void
init_http_post(struct http_post *http_post)
{
	http_post->total_upload_length = 0;
	http_post->uploaded = 0;
	http_post->post_data = NULL;
	http_post->post_fd = -1;
}

/** Free all resources owned by *@a http_post, but do not free the
 * structure itself.  It is safe to call this multiple times.
 *
 * @relates http_post */
void
done_http_post(struct http_post *http_post)
{
	http_post->total_upload_length = 0;
	http_post->uploaded = 0;
	http_post->post_data = NULL;
	if (http_post->post_fd != -1) {
		close(http_post->post_fd);
		http_post->post_fd = -1;
	}
}

/** @relates http_post */
static int
read_http_post_inline(struct http_post *http_post,
		      unsigned char buffer[], int max)
{
	unsigned char *post = http_post->post_data;
	unsigned char *end = strchr(post, FILE_CHAR);
	int total = 0;

	assert(http_post->post_fd < 0);
	if_assert_failed { errno = EINVAL; return -1; }

	if (!end)
		end = strchr(post, '\0');

	while (post < end && total < max) {
		int h1, h2;

		h1 = unhx(post[0]);
		assertm(h1 >= 0 && h1 < 16, "h1 in the POST buffer is %d (%d/%c)", h1, post[0], post[0]);
		if_assert_failed h1 = 0;

		h2 = unhx(post[1]);
		assertm(h2 >= 0 && h2 < 16, "h2 in the POST buffer is %d (%d/%c)", h2, post[0], post[0]);
		if_assert_failed h2 = 0;

		buffer[total++] = (h1<<4) + h2;
		post += 2;
	}
	if (post != end || *end != FILE_CHAR) {
		http_post->post_data = post;
		return total;
	}

	end = strchr(post + 1, FILE_CHAR);
	assert(end);
	*end = '\0';
	http_post->post_fd = open(post + 1, O_RDONLY);
	/* Be careful not to change errno here.  */
	*end = FILE_CHAR;
	if (http_post->post_fd < 0) {
		http_post->post_data = post;
		if (total > 0)
			return total; /* retry the open on the next call */
		else
			return -1; /* caller gets errno from open() */
	}
	http_post->post_data = end + 1;
	return total;
}

/** @relates http_post */
static int
read_http_post_fd(struct http_post *http_post,
		  unsigned char buffer[], int max)
{
	int ret;

	/* safe_read() would set errno = EBADF anyway, but check this
	 * explicitly to make any such bugs easier to detect.  */
	assert(http_post->post_fd >= 0);
	if_assert_failed { errno = EBADF; return -1; }

	ret = safe_read(http_post->post_fd, buffer, max);
	if (ret <= 0) {
		const int errno_from_read = errno;

		close(http_post->post_fd);
		http_post->post_fd = -1;

		errno = errno_from_read;
	}
	return ret;
}

/** Read data from connection.uri->post or from the files to which it
 * refers.
 *
 * @return >0 if read that many bytes; 0 if EOF; -1 on error and set
 * errno.
 *
 * @relates http_post */
int
read_http_post(struct http_post *http_post,
	       unsigned char buffer[], int max)
{
	int total = 0;

	while (total < max) {
		int chunk;
		int post_fd = http_post->post_fd;

		if (post_fd < 0)
			chunk = read_http_post_inline(http_post,
						      buffer + total,
						      max - total);
		else
			chunk = read_http_post_fd(http_post,
						  buffer + total,
						  max - total);
		/* Be careful not to change errno here.  */

		if (chunk == 0 && http_post->post_fd == post_fd)
			return total; /* EOF */
		if (chunk < 0) {
			/* If some data has already been successfully
			 * read to buffer[], tell the caller about
			 * that and forget about the error.  The next
			 * http_read_post_data() call will retry the
			 * operation that failed.  */
			if (total != 0)
				return total;
			else
				return chunk; /* caller gets errno from above */
		}
		total += chunk;
	}
	return total;
}
