#ifndef EL__PROTOCOL_HTTP_HTTP_H
#define EL__PROTOCOL_HTTP_HTTP_H

#include "main/module.h"
#include "protocol/http/blacklist.h"
#include "protocol/http/post.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"

#ifdef __cplusplus
extern "C" {
#endif

struct connection;
struct read_buffer;
struct socket;
struct string;

/* Macros related to this struct are defined in the http.c. */
struct http_version {
	int major;
	int minor;
};

/** connection.info points to this in HTTP and local CGI connections. */
struct http_connection_info {
	blacklist_flags_T bl_flags;
	struct http_version recv_version;
	struct http_version sent_version;

	int close;
	off_t length;
	off_t chunk_remaining;
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

char *subst_user_agent(char *fmt, const char *version,
				char *sysname, char *termsize);
void add_url_to_http_string(struct string *header, struct uri *uri, uri_component_T components);


#ifdef __cplusplus
}
#endif

#endif
