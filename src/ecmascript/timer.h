#ifndef EL__ECMASCRIPT_TIMER_H
#define EL__ECMASCRIPT_TIMER_H

#include <map>

struct timer;

extern std::map<struct timer *, bool> map_timer;

inline void
add_to_map_timer(struct timer *timer)
{
	map_timer[timer] = true;
}

inline void
del_from_map_timer(struct timer *timer)
{
	map_timer.erase(timer);
}

inline bool
check_in_map_timer(struct timer *timer)
{
	return map_timer.find(timer) != map_timer.end();
}

#endif
