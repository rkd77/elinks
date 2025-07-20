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
void gophers_protocol_handler(struct connection *conn);
void gophers_curl_handle_error(struct connection *conn, CURLcode res);
#endif

#ifdef __cplusplus
}
#endif

#endif
