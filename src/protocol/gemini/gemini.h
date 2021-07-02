
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
