
#ifndef EL__PROTOCOL_GOPHER_GOPHER_H
#define EL__PROTOCOL_GOPHER_GOPHER_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef CONFIG_GOPHER
extern protocol_handler_T gopher_protocol_handler;
#else
#define gopher_protocol_handler NULL
#endif

extern struct module gopher_protocol_module;


#endif
