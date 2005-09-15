/* $Id: info.h,v 1.9 2004/11/22 13:27:41 zas Exp $ */

#ifndef EL__DIALOGS_INFO_H
#define EL__DIALOGS_INFO_H

struct session;
struct terminal;

void menu_about(struct terminal *, void *, void *);
void menu_keys(struct terminal *, void *, void *);
void menu_copying(struct terminal *, void *, void *);

void resource_info(struct terminal *term);
void memory_inf(struct terminal *, void *, struct session *);

#endif
