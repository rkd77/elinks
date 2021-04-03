#ifndef EL__MIME_DIALOGS_H
#define EL__MIME_DIALOGS_H

#ifdef __cplusplus
extern "C" {
#endif

struct terminal;

void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#ifdef __cplusplus
}
#endif

#endif
