#ifndef EL__ECMASCRIPT_TIMER_H
#define EL__ECMASCRIPT_TIMER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct timer;

void add_to_map_timer(struct timer *timer);
void del_from_map_timer(struct timer *timer);
bool found_in_map_timer(struct timer *timer);

#ifdef __cplusplus
}
#endif

#endif
