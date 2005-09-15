/* $Id: document.h,v 1.7 2005/06/14 12:25:20 jonas Exp $ */

#ifndef EL__DIALOGS_DOCUMENT_H
#define EL__DIALOGS_DOCUMENT_H

#include "session/session.h"

void nowhere_box(struct terminal *term, unsigned char *title);
void document_info_dialog(struct session *);
void cached_header_dialog(struct session *ses, struct cache_entry *cached);
void protocol_header_dialog(struct session *);

#endif
