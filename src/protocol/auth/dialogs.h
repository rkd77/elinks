
#ifndef EL__PROTOCOL_AUTH_DIALOGS_H
#define EL__PROTOCOL_AUTH_DIALOGS_H

#include "bfu/hierbox.h"

struct session;

void do_auth_dialog(struct session *ses, void *data);
extern struct hierbox_browser auth_browser;
void auth_manager(struct session *);

#endif
