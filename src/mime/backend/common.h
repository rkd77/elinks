
#ifndef EL__MIME_BACKEND_COMMON_H
#define EL__MIME_BACKEND_COMMON_H

#include "mime/mime.h"

struct mime_backend {
	/* Resolve the content type from the @extension. */
	unsigned char *(*get_content_type)(unsigned char *extension);

	/* Given a mime type find a associated handler. The handler can
	 * be given options such */
	struct mime_handler *(*get_mime_handler)(unsigned char *type, int have_x);
};

/* Multiplexor functions for the backends. */

unsigned char *get_content_type_backends(unsigned char *extension);

struct mime_handler *
get_mime_handler_backends(unsigned char *content_type, int have_x);

/* Extracts a filename from @path separated by @separator. Targeted for use
 * with the general unix PATH style strings. */
unsigned char *
get_next_path_filename(unsigned char **path_ptr, unsigned char separator);

struct mime_handler *
init_mime_handler(unsigned char *program, unsigned char *description,
		  unsigned char *backend_name, int ask, int block);

#endif
