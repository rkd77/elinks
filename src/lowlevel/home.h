/* $Id: home.h,v 1.4 2003/05/08 21:50:08 zas Exp $ */

#ifndef EL__LOWLEVEL_HOME_H
#define EL__LOWLEVEL_HOME_H

extern unsigned char *elinks_home;
extern int first_use;

void init_home(void);
void free_home(void);

#endif
