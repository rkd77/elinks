#ifndef EL__PROTOCOL_CURL_HTTP_H
#define EL__PROTOCOL_CURL_HTTP_H

#ifdef CONFIG_LIBCURL
#include <curl/curl.h>
#endif

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEVENT)
struct connection;

extern protocol_handler_T http_curl_protocol_handler;

void http_curl_handle_error(struct connection *conn, CURLcode res);

#endif

#ifdef __cplusplus
}
#endif

#endif
