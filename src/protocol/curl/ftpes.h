#ifndef EL__PROTOCOL_CURL_FTPES_H
#define EL__PROTOCOL_CURL_FTPES_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct module ftpes_protocol_module;

#if defined(CONFIG_FTP) && defined(CONFIG_LIBCURL) && defined(CONFIG_LIBEVENT)
extern protocol_handler_T ftpes_protocol_handler;
#else
#define ftpes_protocol_handler NULL
#endif

#ifdef __cplusplus
}
#endif

#endif
