/* ECMAScript timer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "elinks.h"

#include "js/ecmascript.h"
#include "js/timer.h"
#include "util/string.h"

#include <map>

static std::map<uint32_t, struct ecmascript_timeout *> map_timer;

void
init_map_timer(void)
{
	ELOG
}

void
done_map_timer(void)
{
	ELOG
	map_timer.clear();
}

void
add_to_map_timer(struct ecmascript_timeout *t)
{
	ELOG
	map_timer[t->timeout_id] = t;
}

void
del_from_map_timer(struct ecmascript_timeout *t)
{
	ELOG
	map_timer.erase(t->timeout_id);
}

struct ecmascript_timeout *
find_in_map_timer(uint32_t timeout_id)
{
	ELOG
	auto search = map_timer.find(timeout_id);

	if (search == map_timer.end()) {
		return NULL;
	}
	return search->second;
}
