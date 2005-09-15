/* $Id: gopher.h,v 1.2 2004/10/08 17:19:26 zas Exp $ */

#ifndef EL__PROTOCOL_GOPHER_GOPHER_H
#define EL__PROTOCOL_GOPHER_GOPHER_H

#include "modules/module.h"
#include "protocol/protocol.h"

#ifdef CONFIG_GOPHER
extern protocol_handler gopher_protocol_handler;
#else
#define gopher_protocol_handler NULL
#endif

extern struct module gopher_protocol_module;


#endif
