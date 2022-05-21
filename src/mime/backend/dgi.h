#ifndef EL__MIME_BACKEND_DGI_H
#define EL__MIME_BACKEND_DGI_H

#include "main/module.h"
#include "mime/backend/common.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct mime_backend dgi_mime_backend;
extern struct module dgi_mime_module;

struct mime_handler *get_mime_handler_dgi(char *type, int xwin);


#ifdef __cplusplus
}
#endif

#endif
