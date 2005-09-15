/* $Id: timer.h,v 1.2 2003/05/08 21:50:08 zas Exp $ */

#ifndef EL__LOWLEVEL_TIMER_H
#define EL__LOWLEVEL_TIMER_H

extern int timer_duration;

void reset_timer(void);
void init_timer(void);
void done_timer(void);

#endif
