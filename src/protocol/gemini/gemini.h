#ifndef EL__PROTOCOL_GEMINI_GEMINI_H
#define EL__PROTOCOL_GEMINI_GEMINI_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

struct connection;
struct read_buffer;
struct session;
struct socket;

struct gemini_connection_info {
	char *prompt;
	int code;
};

struct gemini_error_info {
	int code;
	struct uri *uri;
	struct session *ses;
	char *prompt;
	char value[MAX_STR_LEN];
};

extern struct module gemini_protocol_module;

#ifdef CONFIG_GEMINI
extern protocol_handler_T gemini_protocol_handler;
#else
#define gemini_protocol_handler NULL
#endif

#ifdef __cplusplus
}
#endif

#endif
