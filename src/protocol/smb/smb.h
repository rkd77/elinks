/* $Id: smb.h,v 1.6 2004/11/05 03:32:19 jonas Exp $ */

#ifndef EL__PROTOCOL_SMB_SMB_H
#define EL__PROTOCOL_SMB_SMB_H

#include "modules/module.h"
#include "protocol/protocol.h"

extern struct module smb_protocol_module;

#ifdef CONFIG_SMB
extern protocol_handler smb_protocol_handler;
#else
#define smb_protocol_handler NULL
#endif

#endif
