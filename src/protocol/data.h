#ifndef EL__PROTOCOL_DATA_H
#define EL__PROTOCOL_DATA_H

#include "protocol/protocol.h"

#ifdef CONFIG_DATA
extern protocol_handler_T data_protocol_handler;
#else
#define data_protocol_handler NULL
#endif

#endif
