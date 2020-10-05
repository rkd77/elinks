#ifndef EL__PROTOCOL_DATA_H
#define EL__PROTOCOL_DATA_H

#include "protocol/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_DATA
extern protocol_handler_T data_protocol_handler;
#else
#define data_protocol_handler NULL
#endif

#ifdef __cplusplus
}
#endif

#endif
