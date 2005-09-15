/* $Id: dialogs.h,v 1.6 2004/01/07 03:18:19 jonas Exp $ */

#ifndef EL__CACHE_DIALOGS_H
#define EL__CACHE_DIALOGS_H

#include "bfu/hierbox.h"

struct session;
struct terminal;

extern struct hierbox_browser cache_browser;
void cache_manager(struct session *);

#endif
