#ifndef EL__GLOBHIST_DIALOGS_H
#define EL__GLOBHIST_DIALOGS_H

#include "bfu/hierbox.h"
#include "session/session.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct hierbox_browser globhist_browser;
void history_manager(struct session *);

#ifdef __cplusplus
}
#endif

#endif
