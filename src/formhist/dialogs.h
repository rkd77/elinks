#ifndef EL__FORMHIST_DIALOGS_H
#define EL__FORMHIST_DIALOGS_H

#include "bfu/hierbox.h"

#ifdef __cplusplus
extern "C" {
#endif

struct session;

extern struct hierbox_browser formhist_browser;
void formhist_manager(struct session *);

#ifdef __cplusplus
}
#endif

#endif
