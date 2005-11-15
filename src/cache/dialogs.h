#ifndef EL__CACHE_DIALOGS_H
#define EL__CACHE_DIALOGS_H

#include "bfu/hierbox.h"

struct session;
struct terminal;

extern struct hierbox_browser cache_browser;
void cache_manager(struct session *);

#endif
