#ifndef EL__CACHE_DIALOGS_H
#define EL__CACHE_DIALOGS_H

#include "bfu/hierbox.h"

#ifdef __cplusplus
extern "C" {
#endif

struct session;
struct terminal;

extern struct hierbox_browser cache_browser;
void cache_manager(struct session *);

#ifdef __cplusplus
}
#endif

#endif
