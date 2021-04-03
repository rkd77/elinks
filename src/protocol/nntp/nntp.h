
#ifndef EL__PROTOCOL_NNTP_NNTP_H
#define EL__PROTOCOL_NNTP_NNTP_H

#include "main/module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the server that should be used when expanding news: URIs */
char *get_nntp_server(void);

/* Returns the entries the user wants to have shown */
char *get_nntp_header_entries(void);

extern struct module nntp_protocol_module;

#ifdef __cplusplus
}
#endif

#endif
