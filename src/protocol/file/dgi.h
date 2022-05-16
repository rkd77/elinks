
#ifndef EL__PROTOCOL_FILE_DGI_H
#define EL__PROTOCOL_FILE_DGI_H

#ifdef __cplusplus
extern "C" {
#endif

struct connection;
struct module;

extern struct module dgi_protocol_module;
int execute_dgi(struct connection *);

#ifdef __cplusplus
}
#endif

#endif
