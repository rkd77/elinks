#ifndef EL__DIALOGS_TABS_H
#define EL__DIALOGS_TABS_H

#ifdef __cplusplus
extern "C" {
#endif

struct session;
struct terminal;

void free_tabs_data(struct terminal *term);
void tab_manager(struct session *);
void init_hierbox_tab_browser(struct terminal *term);

#ifdef __cplusplus
}
#endif

#endif
