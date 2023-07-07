#ifndef EL__PROTOCOL_FSP_FSP_H
#define EL__PROTOCOL_FSP_FSP_H

#include "main/module.h"
#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct module fsp_protocol_module;

#if defined(CONFIG_FSP) || defined(CONFIG_FSP2)
extern protocol_handler_T fsp_protocol_handler;
#else
#define fsp_protocol_handler NULL
#endif

#ifdef __cplusplus
}
#endif

#endif
