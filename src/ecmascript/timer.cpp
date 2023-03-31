/* ECMAScript timer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <map>
#include <stdlib.h>
#include <stdio.h>

#include "elinks.h"

#include "ecmascript/timer.h"

struct timer;

extern std::map<struct timer *, bool> map_timer;

void
add_to_map_timer(struct timer *timer)
{
	map_timer[timer] = true;
}

void
del_from_map_timer(struct timer *timer)
{
	map_timer.erase(timer);
}

bool
found_in_map_timer(struct timer *timer)
{
	return map_timer.find(timer) != map_timer.end();
}
