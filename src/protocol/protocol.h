#ifndef EL__PROTOCOL_PROTOCOL_H
#define EL__PROTOCOL_PROTOCOL_H

#include "main/module.h"

struct connection;
struct session;
struct terminal;
struct uri;

enum protocol {
	PROTOCOL_ABOUT,
	PROTOCOL_BITTORRENT,
	PROTOCOL_BITTORRENT_PEER,
	PROTOCOL_DATA,
	PROTOCOL_FILE,
	PROTOCOL_FINGER,
	PROTOCOL_FSP,
	PROTOCOL_FTP,
	PROTOCOL_GOPHER,
	PROTOCOL_HTTP,
	PROTOCOL_HTTPS,
	PROTOCOL_JAVASCRIPT,
	PROTOCOL_NEWS,
	PROTOCOL_NNTP,
	PROTOCOL_NNTPS,
	PROTOCOL_PROXY,
	PROTOCOL_SMB,
	PROTOCOL_SNEWS,

	/* Keep these last! */
	PROTOCOL_UNKNOWN,
	PROTOCOL_USER,
	PROTOCOL_LUA,

	/* For protocol backend index checking */
	PROTOCOL_BACKENDS,
};

/* Besides the session an external handler also takes the url as an argument */
typedef void (protocol_handler_T)(struct connection *);
typedef void (protocol_external_handler_T)(struct session *, struct uri *);

/* Accessors for the protocol backends. */

int get_protocol_port(enum protocol protocol);
int get_protocol_need_slashes(enum protocol protocol);
int get_protocol_keep_double_slashes(enum protocol protocol);
int get_protocol_need_slash_after_host(enum protocol protocol);
int get_protocol_free_syntax(enum protocol protocol);
int get_protocol_need_ssl(enum protocol protocol);

protocol_handler_T *get_protocol_handler(enum protocol protocol);
protocol_external_handler_T *get_protocol_external_handler(struct terminal *, struct uri *);

/* Resolves the given protocol @name with length @namelen to a known protocol,
 * PROTOCOL_UNKOWN or PROTOCOL_INVALID if no protocol part could be identified.
 * User defined protocols (configurable via protocol.user) takes precedence. */
enum protocol get_protocol(unsigned char *name, int namelen);

extern struct module protocol_module;

#endif
