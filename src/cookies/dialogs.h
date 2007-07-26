#ifndef EL__COOKIES_DIALOGS_H
#define EL__COOKIES_DIALOGS_H

#include "bfu/hierbox.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/lists.h"

extern LIST_OF(struct cookie) cookie_queries;

void accept_cookie_dialog(struct session *ses, void *data);
extern struct hierbox_browser cookie_browser;
void cookie_manager(struct session *);

#endif
