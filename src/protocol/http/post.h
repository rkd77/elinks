/** Parsing uri.post and uploading files in a POST request.
 * @file */

#ifndef EL__PROTOCOL_HTTP_POST_H
#define EL__PROTOCOL_HTTP_POST_H

#include "network/state.h"

/** State of reading POST data from connection.uri->post and related
 * files.  */
struct http_post {
	/** Total size of the POST body to be uploaded */
	off_t total_upload_length;

	/** Amount of POST body data uploaded so far */
	off_t uploaded;

	/** Points to the next byte to be read from connection.uri->post.
	 * Does not point to const because http_read_post() momentarily
	 * substitutes a null character for the FILE_CHAR at the end of
	 * each file name.  */
	unsigned char *post_data;

	/** File descriptor from which data is being read, or -1 if
	 * none.  */
	int post_fd;
};

void init_http_post(struct http_post *http_post);
void done_http_post(struct http_post *http_post);
int open_http_post(struct http_post *http_post, unsigned char *post_data,
		   unsigned int *files, enum connection_state *error);
int read_http_post(struct http_post *http_post,
		   unsigned char buffer[], int max);

#endif
