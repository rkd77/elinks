#ifndef EL__DIALOGS_DOCUMENT_H
#define EL__DIALOGS_DOCUMENT_H

#include "session/session.h"

#ifdef __cplusplus
extern "C" {
#endif

void nowhere_box(struct terminal *term, unsigned char *title);
void link_info_dialog(struct session *ses);
void document_info_dialog(struct session *);
void cached_header_dialog(struct session *ses, struct cache_entry *cached);
void protocol_header_dialog(struct session *);

#ifdef __cplusplus
}
#endif

#endif
