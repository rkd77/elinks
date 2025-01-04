#ifndef EL__DIALOGS_TABS_H
#define EL__DIALOGS_TABS_H

#include "bfu/hierbox.h"
#include "session/session.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct hierbox_browser tab_browser;
void tab_manager(struct session *);

#ifdef __cplusplus
}
#endif

#endif
