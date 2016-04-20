/** Parsing uri.post and uploading files in a POST request.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
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
	http_post->file_index = 0;
	http_post->file_count = 0;
	http_post->file_read = 0;
	http_post->files = NULL;
}

/** Free all resources owned by *@a http_post, but do not free the
 * structure itself.  It is safe to call this multiple times.
 *
 * @relates http_post */
void
done_http_post(struct http_post *http_post)
{
	size_t i;

	http_post->total_upload_length = 0;
	http_post->uploaded = 0;
	http_post->post_data = NULL;
	if (http_post->post_fd != -1) {
		close(http_post->post_fd);
		http_post->post_fd = -1;
	}
	for (i = 0; i < http_post->file_count; i++)
		mem_free(http_post->files[i].name);
	http_post->file_index = 0;
	http_post->file_count = 0;
	http_post->file_read = 0;
	mem_free_set(&http_post->files, NULL);
}

/** Prepare to read POST data from a URI and possibly to upload files.
 *
 * @param http_post
 *   Must have been initialized with init_http_post().
 * @param[in] post_data
 *   The body of the POST request as formatted by get_form_uri().
 *   However, unlike uri.post, @a post_data must not contain any
 *   Content-Type.  The caller must ensure that the @a post_data
 *   pointer remains valid until done_http_post().
 * @param[out] error
 *   If the function fails, it writes the error state here so that
 *   the caller can pass that on to abort_connection().  If the
 *   function succeeds, the value of *@a error is undefined.
 *
 * This function does not parse the Content-Type from uri.post; the
 * caller must do that.  This is because in local CGI, the child
 * process handles the Content-Type (saving it to an environment
 * variable before exec) but the parent process handles the body of
 * the request (feeding it to the child process via a pipe).
 *
 * @return nonzero on success, zero on error.
 *
 * @relates http_post */
int
open_http_post(struct http_post *http_post, const unsigned char *post_data,
	       struct connection_state *error)
{
	off_t size = 0;
	size_t length = strlen(post_data);
	const unsigned char *end = post_data;

	done_http_post(http_post);
	http_post->post_data = end;

	while (1) {
		struct stat sb;
		const unsigned char *begin;
		int res;
		struct http_post_file *new_files;
		unsigned char *filename;

		begin = strchr((const char *)end, FILE_CHAR);
		if (!begin) break;
		end = strchr((const char *)(begin + 1), FILE_CHAR);
		if (!end) break;
		filename = memacpy(begin + 1, end - begin - 1); /* adds '\0' */
		if (!filename) {
			done_http_post(http_post);
			*error = connection_state(S_OUT_OF_MEM);
			return 0;
		}
		decode_uri(filename);
		res = stat(filename, &sb);
		if (res) {
			*error = connection_state_for_errno(errno);
			done_http_post(http_post);
			return 0;
		}

		/* This use of mem_realloc() in a loop consumes O(n^2)
		 * time but how many files are you really going to
		 * upload in one request?  */
		new_files = mem_realloc(http_post->files,
					(http_post->file_count + 1)
					* sizeof(*new_files));
		if (new_files == NULL) {
			mem_free(filename);
			done_http_post(http_post);
			*error = connection_state(S_OUT_OF_MEM);
			return 0;
		}
		http_post->files = new_files;
		new_files[http_post->file_count].name = filename;
		new_files[http_post->file_count].size = sb.st_size;
		http_post->file_count++;

		size += sb.st_size;
		length -= (end - begin + 1);
		end++;
	}
	size += (length / 2);
	http_post->total_upload_length = size;

	return 1;
}

/** @return -2 if no data was read but the caller should retry;
 * -1 if an error occurred and *@a error was set; 0 at end of data;
 * a positive number if that many bytes were read.
 *
 * @relates http_post */
static int
read_http_post_inline(struct http_post *http_post,
		      unsigned char buffer[], int max,
		      struct connection_state *error)
{
	const unsigned char *post = http_post->post_data;
	const unsigned char *end = strchr((const char *)post, FILE_CHAR);
	int total = 0;

	assert(http_post->post_fd < 0);
	if_assert_failed { *error = connection_state(S_INTERNAL); return -1; }

	if (!end)
		end = strchr((const char *)post, '\0');

	while (post < end && total < max) {
		int h1, h2;

		h1 = unhx(post[0]);
		assertm(h1 >= 0 && h1 < 16, "h1 in the POST buffer is %d (%d/%c)", h1, post[0], post[0]);
		if_assert_failed h1 = 0;

		h2 = unhx(post[1]);
		assertm(h2 >= 0 && h2 < 16, "h2 in the POST buffer is %d (%d/%c)", h2, post[1], post[1]);
		if_assert_failed h2 = 0;

		buffer[total++] = (h1<<4) + h2;
		post += 2;
	}
	if (post != end || *end != FILE_CHAR) {
		http_post->post_data = post;
		return total;
	}

	http_post->file_read = 0;
	end = strchr((const char *)(post + 1), FILE_CHAR);
	assert(end);
	http_post->post_fd = open(http_post->files[http_post->file_index].name,
				  O_RDONLY);
	/* Be careful not to change errno here.  */
	if (http_post->post_fd < 0) {
		http_post->post_data = post;
		if (total > 0)
			return total; /* retry the open on the next call */
		else {
			*error = connection_state_for_errno(errno);
			return -1;
		}
	}
	http_post->post_data = end + 1;
	return total ? total : -2;
}

/** @return -2 if no data was read but the caller should retry;
 * -1 if an error occurred and *@a error was set; 0 at end of data;
 * a positive number if that many bytes were read.
 *
 * @relates http_post */
static int
read_http_post_fd(struct http_post *http_post,
		  unsigned char buffer[], int max,
		  struct connection_state *error)
{
	const struct http_post_file *const file
		= &http_post->files[http_post->file_index];
	int ret;

	/* safe_read() would set errno = EBADF anyway, but check this
	 * explicitly to make any such bugs easier to detect.  */
	assert(http_post->post_fd >= 0);
	if_assert_failed { *error = connection_state(S_INTERNAL); return -1; }

	ret = safe_read(http_post->post_fd, buffer, max);
	if (ret <= 0) {
		const int errno_from_read = errno;

		close(http_post->post_fd);
		http_post->post_fd = -1;
		http_post->file_index++;
		/* http_post->file_read is used below so don't clear it here.
		 * It will be cleared when the next file is opened.  */

		if (ret == -1) {
			*error = connection_state_for_errno(errno_from_read);
			return -1;
		} else if (http_post->file_read != file->size) {
			/* ELinks already sent a Content-Length header
			 * based on the size of this file, but the
			 * file has since been shrunk.  Abort the
			 * connection because ELinks can no longer get
			 * enough data to fill the Content-Length.
			 * (Well, it could pad with zeroes, but that
			 * would be just weird.)  */
			*error = connection_state(S_HTTP_UPLOAD_RESIZED);
			return -1;
		} else {
			/* The upload file ended but there may still
			 * be more data in uri.post.  If not,
			 * read_http_post_inline() will return 0 to
			 * indicate the final end of file.  */
			return -2;
		}
	}

	http_post->file_read += ret;
	if (http_post->file_read > file->size) {
		/* ELinks already sent a Content-Length header based
		 * on the size of this file, but the file has since
		 * been extended.  Abort the connection because ELinks
		 * can no longer fit the entire file in the original
		 * Content-Length.  */
		*error = connection_state(S_HTTP_UPLOAD_RESIZED);
		return -1;
	}

	return ret;
}

/** Read data from connection.uri->post or from the files to which it
 * refers.
 *
 * @return >0 if read that many bytes; 0 if EOF; -1 on error and set
 * *@a error.
 *
 * @relates http_post */
int
read_http_post(struct http_post *http_post,
	       unsigned char buffer[], int max,
	       struct connection_state *error)
{
	int total = 0;

	while (total < max) {
		int chunk;

		if (http_post->post_fd < 0)
			chunk = read_http_post_inline(http_post,
						      buffer + total,
						      max - total,
						      error);
		else
			chunk = read_http_post_fd(http_post,
						  buffer + total,
						  max - total,
						  error);

		if (chunk > 0) {
			total += chunk;
			http_post->uploaded += chunk;
		} else if (chunk != -2) {
			assert(chunk == -1 || chunk == 0);
			/* If some data has already been successfully
			 * read to buffer[], tell the caller about
			 * that and forget about the error.  The next
			 * read_http_post() call will retry the
			 * operation that failed.  */
			if (total != 0)
				return total;
			else
				return chunk;
		}
	}
	return total;
}
