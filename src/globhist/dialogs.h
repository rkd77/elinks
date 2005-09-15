/* $Id: dialogs.h,v 1.7 2004/07/15 00:45:07 jonas Exp $ */

#ifndef EL__GLOBHIST_DIALOGS_H
#define EL__GLOBHIST_DIALOGS_H

#include "bfu/hierbox.h"
#include "sched/session.h"

extern struct hierbox_browser globhist_browser;
void history_manager(struct session *);

#endif
