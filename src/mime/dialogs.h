/* $Id: dialogs.h,v 1.4 2004/06/22 21:49:49 jonas Exp $ */

#ifndef EL__MIME_DIALOGS_H
#define EL__MIME_DIALOGS_H

struct terminal;

void menu_add_ext(struct terminal *, void *, void *);
void menu_del_ext(struct terminal *, void *, void *);
void menu_list_ext(struct terminal *, void *, void *);

#endif
