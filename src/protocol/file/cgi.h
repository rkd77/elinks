
#ifndef EL__PROTOCOL_FILE_CGI_H
#define EL__PROTOCOL_FILE_CGI_H

struct connection;
struct module;

extern struct module cgi_protocol_module;
int execute_cgi(struct connection *);

#endif
