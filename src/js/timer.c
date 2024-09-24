/* ECMAScript timer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include "elinks.h"

#include "js/timer.h"
#include "util/hash.h"
#include "util/string.h"

struct timer;
static struct hash *map_timer;

void
init_map_timer(void)
{
	map_timer = init_hash8();
}

void
done_map_timer(void)
{
	struct hash_item *item;
	int i;

	if (!map_timer) {
		return;
	}

	foreach_hash_item (item, *map_timer, i) {
		mem_free_set(&item->key, NULL);
	}
	free_hash(&map_timer);
}

void
add_to_map_timer(struct timer *timer)
{
	if (map_timer) {
		char *key = memacpy((const char *)&timer, sizeof(timer));

		if (key) {
			add_hash_item(map_timer, key, sizeof(timer), (void *)(intptr_t)1);
		}
	}
}

void
del_from_map_timer(struct timer *timer)
{
	if (map_timer) {
		char *key = memacpy((const char *)&timer, sizeof(timer));

		if (key) {
			struct hash_item *item = get_hash_item(map_timer, key, sizeof(timer));

			if (item) {
				mem_free_set(&item->key, NULL);
				del_hash_item(map_timer, item);
			}
			mem_free(key);
		}
	}
}

bool
found_in_map_timer(struct timer *timer)
{
	bool ret = false;

	if (map_timer) {
		char *key = memacpy((const char *)&timer, sizeof(timer));

		if (key) {
			struct hash_item *item = get_hash_item(map_timer, key, sizeof(timer));

			if (item) {
				ret = true;
			}
			mem_free(key);
		}
	}

	return ret;
}
