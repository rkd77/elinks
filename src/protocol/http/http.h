
#ifndef EL__PROTOCOL_HTTP_HTTP_H
#define EL__PROTOCOL_HTTP_HTTP_H

#include "main/module.h"
#include "protocol/http/blacklist.h"
#include "protocol/protocol.h"

struct connection;
struct read_buffer;
struct socket;

/* Macros related to this struct are defined in the http.c. */
struct http_version {
	int major;
	int minor;
};

/** connection.info points to this in HTTP and local CGI connections. */
struct http_connection_info {
	enum blacklist_flags bl_flags;
	struct http_version recv_version;
	struct http_version sent_version;

	int close;
	int length;
	int chunk_remaining;
	int code;

	/** Total size of the POST body to be uploaded */
	size_t total_upload_length;

	/** Amount of POST body data uploaded so far */
	size_t uploaded;

	/** Points to the next byte to be read from connection.uri->post.
	 * Does not point to const because http_read_post() momentarily
	 * substitutes a null character for the FILE_CHAR at the end of
	 * each file name.  */
	unsigned char *post_data;
};

int http_read_post_data(struct socket *socket,
			unsigned char buffer[], int max);

extern struct module http_protocol_module;

extern protocol_handler_T http_protocol_handler;
extern protocol_handler_T proxy_protocol_handler;

#ifdef CONFIG_SSL
#define https_protocol_handler http_protocol_handler
#else
#define https_protocol_handler NULL
#endif

struct http_connection_info *init_http_connection_info(struct connection *conn, int major, int minor, int close);
void http_got_header(struct socket *, struct read_buffer *);

unsigned char *subst_user_agent(unsigned char *fmt, unsigned char *version,
				unsigned char *sysname, unsigned char *termsize);

#endif
