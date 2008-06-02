
#ifndef EL__PROTOCOL_HTTP_HTTP_H
#define EL__PROTOCOL_HTTP_HTTP_H

#include "main/module.h"
#include "protocol/http/blacklist.h"
#include "protocol/http/post.h"
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

	struct http_post post;
};

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
