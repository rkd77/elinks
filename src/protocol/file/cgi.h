
#ifndef EL__PROTOCOL_FILE_CGI_H
#define EL__PROTOCOL_FILE_CGI_H

#ifdef __cplusplus
extern "C" {
#endif

struct connection;
struct module;

extern struct module cgi_protocol_module;
int execute_cgi(struct connection *);

#ifdef __cplusplus
}
#endif

#endif
