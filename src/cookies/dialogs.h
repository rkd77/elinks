/* $Id: dialogs.h,v 1.7 2005/06/14 12:25:20 jonas Exp $ */

#ifndef EL__COOKIES_DIALOGS_H
#define EL__COOKIES_DIALOGS_H

#include "bfu/hierbox.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/lists.h"

extern struct list_head cookie_queries;

void accept_cookie_dialog(struct session *ses, void *data);
extern struct hierbox_browser cookie_browser;
void cookie_manager(struct session *);

#endif
