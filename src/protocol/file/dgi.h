#ifndef EL__PROTOCOL_FILE_DGI_H
#define EL__PROTOCOL_FILE_DGI_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

struct connection;
struct module;

#ifdef CONFIG_DGI
extern struct module dgi_protocol_module;
extern protocol_handler_T dgi_protocol_handler;
int execute_dgi(struct connection *);
#else
#define dgi_protocol_handler NULL
#endif


#ifdef __cplusplus
}
#endif

#endif
