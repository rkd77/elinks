#ifndef EL__PROTOCOL_PROTOCOL_H
#define EL__PROTOCOL_PROTOCOL_H

#include "main/module.h"

#ifdef __cplusplus
extern "C" {
#endif

struct connection;
struct session;
struct terminal;
struct uri;

enum protocol {
	PROTOCOL_ABOUT,
	PROTOCOL_BITTORRENT,
	PROTOCOL_BITTORRENT_PEER,
	PROTOCOL_DATA,
	PROTOCOL_DGI,
	PROTOCOL_FILE,
	PROTOCOL_FINGER,
	PROTOCOL_FSP,
	PROTOCOL_FTP,
	PROTOCOL_FTPES,
	PROTOCOL_GEMINI,
	PROTOCOL_GOPHER,
	PROTOCOL_HTTP,
	PROTOCOL_HTTPS,
	PROTOCOL_JAVASCRIPT,
	PROTOCOL_MAILCAP,
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

typedef unsigned int protocol_T;

/* Besides the session an external handler also takes the url as an argument */
typedef void (protocol_handler_T)(struct connection *);
typedef void (protocol_external_handler_T)(struct session *, struct uri *);

/* Accessors for the protocol backends. */

int get_protocol_port(protocol_T protocol);
int get_protocol_need_slashes(protocol_T protocol);
int get_protocol_keep_double_slashes(protocol_T protocol);
int get_protocol_need_slash_after_host(protocol_T protocol);
int get_protocol_free_syntax(protocol_T protocol);
int get_protocol_need_ssl(protocol_T protocol);

protocol_handler_T *get_protocol_handler(protocol_T protocol);
protocol_external_handler_T *get_protocol_external_handler(struct terminal *, struct uri *);

/* Resolves the given protocol @name with length @namelen to a known protocol,
 * PROTOCOL_UNKOWN or PROTOCOL_INVALID if no protocol part could be identified.
 * User defined protocols (configurable via protocol.user) takes precedence. */
protocol_T get_protocol(const char *name, int namelen);

extern struct module protocol_module;

#ifdef __cplusplus
}
#endif

#endif
