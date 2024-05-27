#ifndef EL__PROTOCOL_CURL_FTPES_H
#define EL__PROTOCOL_CURL_FTPES_H

#ifdef CONFIG_LIBCURL
#include <curl/curl.h>
#endif

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct module ftpes_protocol_module;

#if defined(CONFIG_FTP) && defined(CONFIG_LIBCURL)
extern protocol_handler_T ftpes_protocol_handler;
void ftp_curl_handle_error(struct connection *conn, CURLcode res);
#else
#define ftpes_protocol_handler NULL
#endif

#ifdef __cplusplus
}
#endif

#endif
