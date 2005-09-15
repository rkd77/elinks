/* $Id: timer.h,v 1.7 2005/06/12 18:42:40 jonas Exp $ */

#ifndef EL__VIEWER_TIMER_H
#define EL__VIEWER_TIMER_H

struct module;

int get_timer_duration(void);
void reset_timer(void);

extern struct module timer_module;

#endif
