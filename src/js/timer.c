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

static struct hash *map_timer;

void
init_map_timer(void)
{
	ELOG
	map_timer = init_hash8();
}

void
done_map_timer(void)
{
	ELOG
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
add_to_map_timer(struct ecmascript_timeout *t)
{
	ELOG
	if (map_timer && t) {
		char *key = memacpy((const char *)&t, sizeof(t));

		if (key) {
			add_hash_item(map_timer, key, sizeof(t), (void *)(intptr_t)1);
		}
	}
}

void
del_from_map_timer(struct ecmascript_timeout *t)
{
	ELOG
	if (map_timer && t) {
		char *key = memacpy((const char *)&t, sizeof(t));

		if (key) {
			struct hash_item *item = get_hash_item(map_timer, key, sizeof(t));

			if (item) {
				mem_free_set(&item->key, NULL);
				del_hash_item(map_timer, item);
			}
			mem_free(key);
		}
	}
}

bool
found_in_map_timer(struct ecmascript_timeout *t)
{
	ELOG
	bool ret = false;

	if (map_timer && t) {
		char *key = memacpy((const char *)&t, sizeof(t));

		if (key) {
			struct hash_item *item = get_hash_item(map_timer, key, sizeof(t));

			if (item) {
				ret = true;
			}
			mem_free(key);
		}
	}

	return ret;
}
