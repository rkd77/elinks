#ifndef EL__DIALOGS_EXMODE_H
#define EL__DIALOGS_EXMODE_H

#include "main/module.h"

#ifdef __cplusplus
extern "C" {
#endif

struct session;

extern struct module exmode_module;

void exmode_start(struct session *ses);

#ifdef __cplusplus
}
#endif

#endif
