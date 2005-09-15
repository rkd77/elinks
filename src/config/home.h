/* $Id: home.h,v 1.6 2005/06/14 17:39:38 jonas Exp $ */

#ifndef EL__CONFIG_HOME_H
#define EL__CONFIG_HOME_H

extern unsigned char *elinks_home;
extern int first_use;

void init_home(void);
void done_home(void);

#endif
