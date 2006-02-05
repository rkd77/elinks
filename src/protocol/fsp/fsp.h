
#ifndef EL__PROTOCOL_FSP_FSP_H
#define EL__PROTOCOL_FSP_FSP_H

#include "main/module.h"
#include "protocol/protocol.h"

extern struct module fsp_protocol_module;

#ifdef CONFIG_FSP
extern protocol_handler_T fsp_protocol_handler;
#else
#define fsp_protocol_handler NULL
#endif

#endif
