#ifndef EL__DIALOGS_EXMODE_H
#define EL__DIALOGS_EXMODE_H

#include "main/module.h"

struct session;

extern struct module exmode_module;

void exmode_start(struct session *ses);

#endif
