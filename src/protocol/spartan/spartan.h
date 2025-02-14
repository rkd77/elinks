#ifndef EL__PROTOCOL_SPARTAN_SPARTAN_H
#define EL__PROTOCOL_SPARTAN_SPARTAN_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

struct connection;
struct read_buffer;
struct session;
struct socket;

struct spartan_connection_info {
	char *msg;
	int code;
};

struct spartan_error_info {
	int code;
	struct uri *uri;
	struct session *ses;
	char *msg;
};

extern struct module spartan_protocol_module;

#ifdef CONFIG_SPARTAN
extern protocol_handler_T spartan_protocol_handler;
#else
#define spartan_protocol_handler NULL
#endif

#ifdef __cplusplus
}
#endif

#endif
