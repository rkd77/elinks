#ifndef EL__MIME_MIME_H
#define EL__MIME_MIME_H

#include "main/module.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cache_entry;
struct uri;

struct mime_handler {
	char *description;
	const char *backend_name;
	char *inpext;
	char *outext;
	unsigned int ask:1;
	unsigned int block:1;
	unsigned int copiousoutput:1;
	unsigned int dgi:1;
	char program[1]; /* XXX: Keep last! */
};

extern struct module mime_module;

/* Guess content type of the document. Either from the protocol header or
 * scanning the uri for extensions. */
char *get_content_type(struct cache_entry *cached);

/* Guess content type by looking at configurations of the given @extension */
char *get_extension_content_type(char *extension);

/* Find program to handle mimetype. The @xwin tells about X capabilities. */
struct mime_handler *
get_mime_type_handler(char *content_type, int xwin);

/* Extracts strictly the filename part (the crap between path and query) and
 * adds it to the @string. Note that there are cases where the string will be
 * empty ("") (ie. http://example.com/?crash=elinks). */
struct string *add_mime_filename_to_string(struct string *string, struct uri *uri);

#ifdef __cplusplus
}
#endif

#endif
