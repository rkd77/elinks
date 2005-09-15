/* $Id: http.h,v 1.14.2.1 2005/01/04 01:08:03 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_HTTP_H
#define EL__PROTOCOL_HTTP_HTTP_H

#include "lowlevel/connect.h"
#include "modules/module.h"
#include "protocol/http/blacklist.h"
#include "protocol/protocol.h"
#include "sched/connection.h"

extern struct module http_protocol_module;

struct http_version {
	int major;
	int minor;
};

#define HTTP_0_9(x)	 ((x).major == 0 && (x).minor == 9)
#define PRE_HTTP_1_0(x)  ((x).major < 1)
#define HTTP_1_0(x)	 ((x).major == 1 && (x).minor == 0)
#define POST_HTTP_1_0(x) ((x).major > 1 || ((x).major == 1 && (x).minor > 0))
#define PRE_HTTP_1_1(x)  (PRE_HTTP_1_0(x) || HTTP_1_0(x))
#define HTTP_1_1(x)	 ((x).major == 1 && (x).minor == 1)
#define POST_HTTP_1_1(x) ((x).major > 2 || ((x).major == 1 && (x).minor > 1))


struct http_connection_info {
	enum blacklist_flags bl_flags;
	struct http_version recv_version;
	struct http_version sent_version;

	int close;

#define LEN_CHUNKED -2 /* == we get data in unknown number of chunks */
#define LEN_FINISHED 0
	int length;

	/* Either bytes coming in this chunk yet or "parser state". */
#define CHUNK_DATA_END	-3
#define CHUNK_ZERO_SIZE	-2
#define CHUNK_SIZE	-1
	int chunk_remaining;

	int http_code;
};

extern protocol_handler http_protocol_handler;
extern protocol_handler proxy_protocol_handler;

#ifdef CONFIG_SSL
#define https_protocol_handler http_protocol_handler
#else
#define https_protocol_handler NULL
#endif

void http_got_header(struct connection *, struct read_buffer *);

unsigned char *subst_user_agent(unsigned char *fmt, unsigned char *version,
				unsigned char *sysname, unsigned char *termsize);

#endif
