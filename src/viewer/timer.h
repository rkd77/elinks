#ifndef EL__VIEWER_TIMER_H
#define EL__VIEWER_TIMER_H

struct module;

int get_timer_duration(void);
void reset_timer(void);

extern struct module timer_module;

#endif
