/* $Id: data.h,v 1.2 2004/08/18 14:11:51 jonas Exp $ */

#ifndef EL__PROTOCOL_DATA_H
#define EL__PROTOCOL_DATA_H

#include "protocol/protocol.h"

#ifdef CONFIG_DATA
extern protocol_handler data_protocol_handler;
#else
#define data_protocol_handler NULL
#endif

#endif
