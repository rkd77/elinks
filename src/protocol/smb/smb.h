
#ifndef EL__PROTOCOL_SMB_SMB_H
#define EL__PROTOCOL_SMB_SMB_H

#include "main/module.h"
#include "protocol/protocol.h"

extern struct module smb_protocol_module;

#ifdef CONFIG_SMB
extern protocol_handler_T smb_protocol_handler;
#else
#define smb_protocol_handler NULL
#endif

#endif
