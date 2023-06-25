#ifndef EL__PROTOCOL_CURL_HTTP_H
#define EL__PROTOCOL_CURL_HTTP_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEVENT)
struct connection;

extern protocol_handler_T http_curl_protocol_handler;

char *http_curl_check_redirect(struct connection *conn);

#endif

#ifdef __cplusplus
}
#endif

#endif
