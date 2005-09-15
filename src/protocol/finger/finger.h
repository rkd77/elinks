/* $Id: finger.h,v 1.1.6.1 2005/02/28 02:06:41 jonas Exp $ */

#ifndef EL__PROTOCOL_FINGER_FINGER_H
#define EL__PROTOCOL_FINGER_FINGER_H

#include "modules/module.h"
#include "protocol/protocol.h"

#ifdef CONFIG_FINGER
extern protocol_handler finger_protocol_handler;
#else
#define finger_protocol_handler NULL
#endif

extern struct module finger_protocol_module;


#endif
