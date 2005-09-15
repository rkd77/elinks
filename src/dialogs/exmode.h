/* $Id: exmode.h,v 1.4 2004/07/04 14:03:37 jonas Exp $ */

#ifndef EL__DIALOGS_EXMODE_H
#define EL__DIALOGS_EXMODE_H

#include "modules/module.h"

struct session;

extern struct module exmode_module;

void exmode_start(struct session *ses);

#endif
