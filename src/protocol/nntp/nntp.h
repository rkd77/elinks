
#ifndef EL__PROTOCOL_NNTP_NNTP_H
#define EL__PROTOCOL_NNTP_NNTP_H

#include "main/module.h"

/* Returns the server that should be used when expanding news: URIs */
unsigned char *get_nntp_server(void);

/* Returns the entries the user wants to have shown */
unsigned char *get_nntp_header_entries(void);

extern struct module nntp_protocol_module;

#endif
