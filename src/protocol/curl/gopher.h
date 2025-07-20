#ifndef EL__PROTOCOL_CURL_GOPHER_H
#define EL__PROTOCOL_CURL_GOPHER_H

#ifdef CONFIG_LIBCURL
#include <curl/curl.h>
#endif

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct module gopher_protocol_module;

#if defined(CONFIG_GOPHER) && defined(CONFIG_LIBCURL)
extern struct module gophers_protocol_module;
extern protocol_handler_T gophers_protocol_handler;
void gophers_curl_handle_error(struct connection *conn, CURLcode res);
#else
#define gophers_protocol_handler NULL
#endif

#ifdef __cplusplus
}
#endif

#endif
