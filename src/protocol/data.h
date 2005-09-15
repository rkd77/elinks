/* $Id: data.h,v 1.3 2005/03/05 21:04:49 jonas Exp $ */

#ifndef EL__PROTOCOL_DATA_H
#define EL__PROTOCOL_DATA_H

#include "protocol/protocol.h"

#ifdef CONFIG_DATA
extern protocol_handler_T data_protocol_handler;
#else
#define data_protocol_handler NULL
#endif

#endif
