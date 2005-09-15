/* $Id: dialogs.h,v 1.3 2004/05/29 04:25:24 jonas Exp $ */

#ifndef EL__FORMHIST_DIALOGS_H
#define EL__FORMHIST_DIALOGS_H

#include "bfu/hierbox.h"

struct session;

extern struct hierbox_browser formhist_browser;
void formhist_manager(struct session *);

#endif
