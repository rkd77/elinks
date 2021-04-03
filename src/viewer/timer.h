#ifndef EL__VIEWER_TIMER_H
#define EL__VIEWER_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

struct module;

int get_timer_duration(void);
void reset_timer(void);

extern struct module timer_module;

#ifdef __cplusplus
}
#endif

#endif
