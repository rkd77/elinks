/** Parsing uri.post and uploading files in a POST request.
 * @file */

#ifndef EL__PROTOCOL_HTTP_POST_H
#define EL__PROTOCOL_HTTP_POST_H

#include "network/state.h"

/** Information about a file to be uploaded in a POST request.
 * open_http_post() collects this information and done_http_post()
 * discards it.  */
struct http_post_file {
	/** The name of the file.  Must be freed with mem_free().  */
	unsigned char *name;

	/** The size of the file.  */
	off_t size;
};

/** State of reading POST data from connection.uri->post and related
 * files.  */
struct http_post {
	/** Total size of the POST body to be uploaded */
	off_t total_upload_length;

	/** Amount of POST body data uploaded so far.
	 * read_http_post() increments this.  */
	off_t uploaded;

	/** Points to the next byte to be read from
	 * connection.uri->post.  */
	const unsigned char *post_data;

	/** File descriptor from which data is being read, or -1 if
	 * none.  */
	int post_fd;

	/** Current position in the #files array.  This is the file
	 * that is currently being read (when post_fd != -1) or would
	 * be read next (when post_fd == -1).  */
	size_t file_index;

	/** Number of files to be uploaded, i.e. the number of
	 * elements in the #files array.  */
	size_t file_count;

	/** Number of bytes read from the current file so far.
	 * The value makes sense only when post_fd != -1.  */
	off_t file_read;

	/** Array of information about files to be uploaded.  */
	struct http_post_file *files;
};

void init_http_post(struct http_post *http_post);
void done_http_post(struct http_post *http_post);
int open_http_post(struct http_post *http_post, const unsigned char *post_data,
		   struct connection_state *error);
int read_http_post(struct http_post *http_post,
		   unsigned char buffer[], int max,
		   struct connection_state *error);

#endif
