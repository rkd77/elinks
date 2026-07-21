#ifndef EL__PROTOCOL_FILE_MAILCAP_H
#define EL__PROTOCOL_FILE_MAILCAP_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct module mailcap_protocol_module;
extern struct module mailcap_html_protocol_module;

#ifdef CONFIG_MAILCAP
extern protocol_handler_T mailcap_protocol_handler;
extern protocol_handler_T mailcap_html_protocol_handler;
#else

#define mailcap_protocol_handler NULL
#define mailcap_html_protocol_handler NULL

#endif

#ifdef __cplusplus
}
#endif

#endif
