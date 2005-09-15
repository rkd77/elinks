/* $Id: dialogs.h,v 1.8 2005/06/14 12:25:20 jonas Exp $ */

#ifndef EL__GLOBHIST_DIALOGS_H
#define EL__GLOBHIST_DIALOGS_H

#include "bfu/hierbox.h"
#include "session/session.h"

extern struct hierbox_browser globhist_browser;
void history_manager(struct session *);

#endif
