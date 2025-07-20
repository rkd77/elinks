
#ifndef EL__PROTOCOL_GOPHER_GOPHER_H
#define EL__PROTOCOL_GOPHER_GOPHER_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

struct string;

#ifdef CONFIG_GOPHER
extern protocol_handler_T gopher_protocol_handler;

void add_gopher_menu_line(struct string *buffer, char *line);
char *get_gopher_line_end(char *data, int datalen);

#else
#define gopher_protocol_handler NULL
#endif

extern struct module gopher_protocol_module;

#ifdef __cplusplus
}
#endif

#endif
