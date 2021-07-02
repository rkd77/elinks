
#ifndef EL__PROTOCOL_GEMINI_GEMINI_H
#define EL__PROTOCOL_GEMINI_GEMINI_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

struct connection;
struct read_buffer;
struct socket;

struct gemini_connection_info {
	int code;
};

extern struct module gemini_protocol_module;

extern protocol_handler_T gemini_protocol_handler;

struct gemini_connection_info *init_gemini_connection_info(struct connection *conn);
void gemini_got_header(struct socket *, struct read_buffer *);

#ifdef __cplusplus
}
#endif

#endif
