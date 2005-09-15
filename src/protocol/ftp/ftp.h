/* $Id: ftp.h,v 1.8 2004/10/12 14:26:48 zas Exp $ */

#ifndef EL__PROTOCOL_FTP_FTP_H
#define EL__PROTOCOL_FTP_FTP_H

#include "modules/module.h"
#include "protocol/protocol.h"

extern struct module ftp_protocol_module;

#ifdef CONFIG_FTP
extern protocol_handler ftp_protocol_handler;
#else
#define ftp_protocol_handler NULL
#endif

#endif
