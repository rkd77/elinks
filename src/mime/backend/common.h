
#ifndef EL__MIME_BACKEND_COMMON_H
#define EL__MIME_BACKEND_COMMON_H

#include "mime/mime.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mime_backend {
	/* Resolve the content type from the @extension. */
	char *(*get_content_type)(char *extension);

	/* Given a mime type find a associated handler. The handler can
	 * be given options such */
	struct mime_handler *(*get_mime_handler)(char *type, int have_x);
};

/* Multiplexor functions for the backends. */

char *get_content_type_backends(char *extension);

struct mime_handler *
get_mime_handler_backends(char *content_type, int have_x);

/* Extracts a filename from @path separated by @separator. Targeted for use
 * with the general unix PATH style strings. */
char *
get_next_path_filename(char **path_ptr, unsigned char separator);

struct mime_handler *
init_mime_handler(char *program, char *description,
		  char *backend_name, int ask, int block);

#ifdef __cplusplus
}
#endif

#endif
